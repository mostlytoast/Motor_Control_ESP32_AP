/*
  PN532 Card Identifier + Capability Analyzer V002
  ESP32 / PN532 / Hardware SPI

  Wiring:
    SCK=18, MISO=19, MOSI=23, CS=5, RST not connected.

  Stage 1 - Card identification:
    - ISO14443A identification using the NXP AN10833 flow
    - Native and ISO14443-4 GetVersion where applicable
    - ATS / historical-byte analysis
    - Ultralight-C AUTH part-1 probe
    - FeliCa / NFC Type F detection

  Stage 2 - Magic-card capability analysis:
    - Gen1A / Gen1B 40/43 backdoor
    - Direct/no-auth block-0 writable style
    - Gen2 / CUID authenticated block-0 writable style
    - Gen3 7-byte same-UID command
    - Per-probe Yes / No / Not tested reporting
    - Final 'Not a supported magic card' conclusion when no probe matches

  Safety:
    - Gen1 and identification tests are read-only.
    - Direct and Gen2 tests write the existing block 0 back unchanged.
    - Gen3 sends the existing 7-byte UID back unchanged.
    - Magic probes run only on Classic/Plus-looking ISO14443A branches.
*/

#include "Utils.h"
#include "PN532.h"

#define PN532_CS_PIN     5
#define PN532_RESET_PIN  255

#define MAX_VERSION_LEN  40

#define CARD_TYPE_FELICA_212KBPS  0x01
#define CARD_TYPE_FELICA_424KBPS  0x02


struct VersionInfo {
  bool supported;
  byte raw[MAX_VERSION_LEN];
  byte rawLength;
  bool desfireStyleStatus;
  byte finalStatus;
};

enum MagicResult {
  MAGIC_NONE,
  MAGIC_GEN1,
  MAGIC_GEN2,
  MAGIC_GEN3,
  MAGIC_DIRECT
};

enum ProbeResult {
  PROBE_MATCH,
  PROBE_NO_MATCH,
  PROBE_NOT_TESTED
};

struct MagicAnalysis {
  ProbeResult gen1;
  ProbeResult direct;
  ProbeResult gen2;
  ProbeResult gen3;
  MagicResult conclusion;
};


// ---------- forward declarations ----------

void printHexByte(byte b);
void printHexBuf(const byte *buf, byte len);

bool tryRatsGetAts(Iso14443AInfo &info);
void printAtsInfo(const Iso14443AInfo &info);
void checkMifarePlusHistoricalBytes(const Iso14443AInfo &info);
bool getVersionL4Wrapped(VersionInfo &vi);

const char *storageHint(byte sizeByte);
const char *ultralightFromGetVersion(byte subType, byte major, byte sizeByte);
const char *ntag21xFromStorage(byte storageByte);

// ---------- PN532 wrapper ----------

PN532 nfc;
uint64_t lastUid = 0;

// ---------- print helpers ----------

void printHexByte(byte b) {
  if (b < 0x10) Serial.print('0');
  Serial.print(b, HEX);
}

void printHexBuf(const byte *buf, byte len) {
  for (byte i = 0; i < len; i++) {
    printHexByte(buf[i]);
    if (i + 1 < len) Serial.print(' ');
  }
}

uint64_t uidTo64(const byte *uid, byte len) {
  uint64_t v = 0;
  byte n = len > 8 ? 8 : len;
  memcpy(&v, uid, n);
  return v;
}

bool isAck0A(const byte *rx, byte rxLen) {
  if (rxLen < 1) return false;
  return ((rx[0] & 0x0F) == 0x0A);
}

void restoreMagicRegisters() {
  byte restoreRegs[] = {
    0x63, 0x02, 0x80,
    0x63, 0x03, 0x80,
    0x63, 0x3D, 0x00
  };

  nfc.writeRegisterBytes(restoreRegs, sizeof(restoreRegs));
}

// ---------- FeliCa ----------

void printFelicaCard(const FelicaInfo &fi) {
  Serial.println();
  Serial.println("================ FELICA CARD ================");

  Serial.print("Bitrate: ");
  if (fi.bitrate == CARD_TYPE_FELICA_212KBPS) Serial.println("212 kbps");
  else if (fi.bitrate == CARD_TYPE_FELICA_424KBPS) Serial.println("424 kbps");
  else Serial.println("unknown");

  Serial.print("IDm: ");
  printHexBuf(fi.idm, 8);
  Serial.println();

  Serial.print("PMm: ");
  printHexBuf(fi.pmm, 8);
  Serial.println();

  Serial.print("System code: 0x");
  printHexByte(fi.systemCode >> 8);
  printHexByte(fi.systemCode & 0xFF);
  Serial.println();

  if (fi.systemCode == 0x88B4) {
    Serial.println("  Flow result: FeliCa Lite-S / NFC Type F");
  } else {
    Serial.println("  Flow result: FeliCa / NFC Type F");
  }

  Serial.println("=============================================");
}

// ---------- SAK ISO bit helpers ----------
// AN10833 uses ISO bit numbering. LSBit is bit 1.

bool sakIsoBit(byte sak, byte isoBitNumber) {
  return (sak & (1 << (isoBitNumber - 1))) != 0;
}

bool sakBit1(byte sak) { return sakIsoBit(sak, 1); }
bool sakBit2(byte sak) { return sakIsoBit(sak, 2); }
bool sakBit4(byte sak) { return sakIsoBit(sak, 4); }
bool sakBit5(byte sak) { return sakIsoBit(sak, 5); }
bool sakBit6(byte sak) { return sakIsoBit(sak, 6); }
bool sakBit7(byte sak) { return sakIsoBit(sak, 7); }

bool sakIso14443_4(byte sak) {
  return sakBit6(sak);
}

// ---------- GetVersion decoding ----------

const char *storageHint(byte sizeByte) {
  switch (sizeByte) {
    case 0x0B: return "about 48 B";
    case 0x0E: return "about 128 B";

    case 0x0F: return "about 144 B user memory";
    case 0x11: return "about 504 B user memory";
    case 0x13: return "about 888 B user memory";

    case 0x10: return "about 256 B";
    case 0x16: return "about 2 KB";
    case 0x18: return "about 4 KB";
    case 0x1A: return "about 8 KB";
    case 0x1C: return "about 16 KB";
    case 0x1E: return "about 32 KB";

    default:   return "see size coding";
  }
}

const char *ultralightFromGetVersion(byte subType, byte major, byte sizeByte) {
  // Type 2 GET_VERSION layout:
  // raw[2] = product type 0x03 = MIFARE Ultralight
  // raw[3] = product subtype
  // raw[4] = major version
  // raw[6] = storage size

  if (major == 0x04 && sizeByte == 0x0F) {
    if (subType == 0x01) return "MIFARE Ultralight AES 17 pF";
    if (subType == 0x02) return "MIFARE Ultralight AES 50 pF";
    return "MIFARE Ultralight AES";
  }

  if (major == 0x01) {
    switch (sizeByte) {
      case 0x0B: return "MIFARE Ultralight EV1 48 byte";
      case 0x0E: return "MIFARE Ultralight EV1 128 byte";
      default:   return "MIFARE Ultralight EV1";
    }
  }

  return "MIFARE Ultralight family";
}

const char *ntag21xFromStorage(byte storageByte) {
  switch (storageByte) {
    case 0x0F: return "NTAG213";
    case 0x11: return "NTAG215";
    case 0x13: return "NTAG216";
    default:   return "NTAG 2xx family";
  }
}

const char *productFamilyFromType(byte hwType) {
  switch (hwType & 0x0F) {
    case 0x01: return "MIFARE DESFire / MIFARE DUOX";
    case 0x02: return "MIFARE Plus";
    case 0x03: return "MIFARE Ultralight";
    case 0x04: return "NTAG";
    case 0x07: return "NTAG I2C";
    case 0x08: return "MIFARE DESFire Light";
    default:   return "Unknown/RFU";
  }
}

const char *ntag4xxSizeHint(const VersionInfo &vi) {
  if (vi.rawLength >= 7) {
    byte hwType  = vi.raw[1];
    byte hwSub   = vi.raw[2];
    byte hwMajor = vi.raw[3];
    byte size    = vi.raw[5];

    if (hwType == 0x04 && hwSub == 0x02 && hwMajor == 0x30 && size == 0x11) {
      return "416 B EEPROM user file memory / NTAG424 DNA";
    }
  }

  return "NTAG 4xx Type 4 memory coding";
}

void printVersionConclusion(const Iso14443AInfo &info, const VersionInfo &vi) {
  if (!(vi.supported && vi.rawLength >= 7)) {
    Serial.println("  GetVersion: no valid response");
    return;
  }

  byte hwType;
  byte hwSub;
  byte hwMajor;
  byte sizeByte;

  if (sakIso14443_4(info.sak)) {
  // DESFire / ISO14443-4 style version layout.
  hwType = vi.raw[1];
  hwSub = vi.raw[2];
  hwMajor = vi.raw[3];
  sizeByte = vi.raw[5];
} else {
  // Type 2 GET_VERSION layout:
  // [0]=fixed/vendor, [1]=vendor/product, [2]=product type,
  // [3]=subtype, [4]=major, [5]=minor, [6]=storage.
  hwType = vi.raw[2];
  hwSub = vi.raw[3];
  hwMajor = vi.raw[4];
  sizeByte = vi.raw[6];
}

  byte family = hwType & 0x0F;

  Serial.print("  GetVersion raw: ");
  printHexBuf(vi.raw, vi.rawLength);
  Serial.println();

  Serial.print("  Product family: ");
  Serial.println(productFamilyFromType(hwType));

if (family == 0x04 && sakIso14443_4(info.sak)) {
  Serial.print("  Size hint: ");
  Serial.println(ntag4xxSizeHint(vi));
} else {
  Serial.print("  Size hint: ");
  Serial.println(storageHint(sizeByte));
}

  switch (family) {
    case 0x01:
      if (hwMajor == 0xA0) {
        Serial.println("  Flow result: MIFARE DUOX family");
      } else {
        Serial.print("  Flow result: MIFARE DESFire ");

        if      (hwMajor == 0x11) Serial.print("EV1");
        else if (hwMajor == 0x12) Serial.print("EV2");
        else if (hwMajor == 0x13) Serial.print("EV3");
        else {
          Serial.print("EVx / generation 0x");
          printHexByte(hwMajor);
        }

        Serial.print(", ");
        Serial.println(storageHint(sizeByte));
      }
      break;

    case 0x02:
  Serial.print("  Flow result: MIFARE Plus");

  if      (hwMajor == 0x11) Serial.print(" EV1");
  else if (hwMajor == 0x12) Serial.print(" EV2?");
  else if (hwMajor == 0x13) Serial.print(" EV3?");
  else if (hwMajor == 0x21) Serial.print(" EV1");
  else if (hwMajor == 0x22) Serial.print(" EV2");
  else if (hwMajor == 0x23) Serial.print(" EV3");
  else {
    Serial.print(" generation 0x");
    printHexByte(hwMajor);
  }

  Serial.print(", ");
  Serial.println(storageHint(sizeByte));
  break;

    case 0x03:
      Serial.print("  Flow result: ");
      Serial.println(ultralightFromGetVersion(hwSub, hwMajor, sizeByte));
      break;

    case 0x04:
  if (hwMajor == 0xA0) {
    Serial.println("  Flow result: NTAG X DNA family");
  } else if (sakIso14443_4(info.sak)) {
    if (vi.rawLength >= 3 && vi.raw[2] == 0x02 && hwMajor == 0x30) {
      Serial.println("  Flow result: NTAG 424 DNA / NTAG 4xx Type 4 tag family");
    } else {
      Serial.println("  Flow result: NTAG 4xx / Type 4 tag family");
    }
  } else {
    Serial.print("  Flow result: ");
    Serial.println(ntag21xFromStorage(sizeByte));
  }
  break;
      

    case 0x07:
      Serial.println("  Flow result: NTAG I2C / NTAG I2C plus family");
      break;

    case 0x08:
      Serial.println("  Flow result: MIFARE DESFire Light");
      break;

    default:
      Serial.println("  Flow result: product family not decoded");
      break;
  }
}

// Native GetVersion for Type 2 / Ultralight / NTAG path.
bool rawGetVersion(VersionInfo &vi) {
  memset(&vi, 0, sizeof(vi));

  byte rx[32];
  byte rxLen = 0;
  byte cmdGetVersion[] = {0x60};

  // For Type 2 native commands, use InCommunicateThru.
  if (!nfc.communicateThru(cmdGetVersion, sizeof(cmdGetVersion), rx, &rxLen, sizeof(rx))) {
    return false;
  }

  if (rxLen < 7) {
    return false;
  }

  vi.supported = true;

  byte copyLen = rxLen;
  if (copyLen > MAX_VERSION_LEN) copyLen = MAX_VERSION_LEN;

  memcpy(vi.raw, rx, copyLen);
  vi.rawLength = copyLen;

  return true;
}

// Wrapped APDU GetVersion for ISO14443-4 cards.
bool getVersionL4Wrapped(VersionInfo &vi) {
  memset(&vi, 0, sizeof(vi));

  byte rx[40];
  byte rxLen = 0;
  byte cmdGetVersion[] = {0x90, 0x60, 0x00, 0x00, 0x00};

  if (!nfc.dataExchange(cmdGetVersion, sizeof(cmdGetVersion), rx, &rxLen, sizeof(rx))) {
    return false;
  }

  if (rxLen < 2 || rx[rxLen - 2] != 0x91) {
    return false;
  }

  vi.supported = true;
  vi.desfireStyleStatus = true;

  bool more = true;

  while (more) {
    byte status = rx[rxLen - 1];
    vi.finalStatus = status;

    byte dataLen = rxLen - 2;
    if (dataLen > MAX_VERSION_LEN - vi.rawLength) {
      dataLen = MAX_VERSION_LEN - vi.rawLength;
    }

    memcpy(vi.raw + vi.rawLength, rx, dataLen);
    vi.rawLength += dataLen;

    if (status == 0x00) {
      more = false;
      break;
    }

    if (status != 0xAF || vi.rawLength >= MAX_VERSION_LEN) {
      more = false;
      break;
    }

    byte cmdMore[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    rxLen = 0;

    if (!nfc.dataExchange(cmdMore, sizeof(cmdMore), rx, &rxLen, sizeof(rx))) {
      break;
    }

    if (rxLen < 2 || rx[rxLen - 2] != 0x91) {
      break;
    }
  }

  return vi.rawLength >= 7;
}

// ---------- Ultralight-C probe ----------

bool tryUltralightCAuthProbe() {
  nfc.SwitchOffRfField();
  delay(250);

  Iso14443AInfo tmp;
  if (!nfc.readIso14443A(tmp) || !tmp.present) {
    Serial.println("  Ultralight-C AUTH probe: card reselect failed");
    return false;
  }

  byte cmdAuth1[] = {0x1A, 0x00};
  byte rx[20] = {0};
  byte rxLen = 0;

  if (!nfc.communicateThru(cmdAuth1, sizeof(cmdAuth1), rx, &rxLen, sizeof(rx))) {
    Serial.println("  Ultralight-C AUTH probe: no valid response");
    return false;
  }

  Serial.print("  Ultralight-C AUTH response: ");
  printHexBuf(rx, rxLen);
  Serial.println();

  return (rxLen >= 9 && rx[0] == 0xAF);
}

// ---------- ATS helpers ----------

byte getAtsHistoricalOffset(const Iso14443AInfo &info) {
  if (info.atsLength < 2) return 0;

  byte t0 = info.ats[1];
  byte offset = 2;

  if (t0 & 0x10) offset++;
  if (t0 & 0x20) offset++;
  if (t0 & 0x40) offset++;

  if (offset >= info.atsLength) return 0;
  return offset;
}

void printHistoricalBytes(const Iso14443AInfo &info) {
  byte offset = getAtsHistoricalOffset(info);

  if (offset == 0) {
    Serial.println("  Historical bytes: none");
    return;
  }

  Serial.print("  Historical bytes: ");
  printHexBuf(info.ats + offset, info.atsLength - offset);
  Serial.println();
}

void printAtsInfo(const Iso14443AInfo &info) {
  Serial.println("ATS / RATS:");

  if (!info.atsLength) {
    Serial.println("  No ATS in activation response");
    return;
  }

  Serial.print("  ATS: ");
  printHexBuf(info.ats, info.atsLength);
  Serial.println();

  if (info.atsLength >= 2) {
    Serial.print("  TL: ");
    Serial.println(info.ats[0]);

    Serial.print("  T0: 0x");
    printHexByte(info.ats[1]);
    Serial.println();

    Serial.print("  FSCI: ");
    Serial.println(info.ats[1] & 0x0F);

    printHistoricalBytes(info);
  }
}

bool histEquals(const Iso14443AInfo &info, const byte *pattern, byte patternLen) {
  byte offset = getAtsHistoricalOffset(info);
  if (offset == 0) return false;

  byte histLen = info.atsLength - offset;
  if (histLen != patternLen) return false;

  return memcmp(info.ats + offset, pattern, patternLen) == 0;
}

const char *plusSizeFromAtqa(uint16_t atqa) {
  switch (atqa & 0x000F) {
    case 0x04: return "2K";
    case 0x02: return "4K";
    default:   return "unknown size";
  }
}

void checkMifarePlusHistoricalBytes(const Iso14443AInfo &info) {
  static const byte PLUS_S[]  = {0xC1, 0x05, 0x2F, 0x2F, 0x00, 0x35, 0xC7};
  static const byte PLUS_X[]  = {0xC1, 0x05, 0x2F, 0x2F, 0x01, 0xBC, 0xD6};
  static const byte PLUS_SE[] = {0xC1, 0x05, 0x21, 0x30, 0x00, 0x77, 0xC1};

  if (histEquals(info, PLUS_SE, sizeof(PLUS_SE))) {
    Serial.println("  Flow result: MIFARE Plus SE 1K, 7-byte UID");
    return;
  }

  if (histEquals(info, PLUS_S, sizeof(PLUS_S))) {
  Serial.print("  Flow result: MIFARE Plus S ");
  Serial.println(plusSizeFromAtqa(info.atqa));
  return;
}

  if (histEquals(info, PLUS_X, sizeof(PLUS_X))) {
  Serial.print("  Flow result: MIFARE Plus X ");
  Serial.println(plusSizeFromAtqa(info.atqa));
  return;
}

  Serial.println("  Flow result: ISO14443-4 card, no decoded AN10833 historical-byte match");
}

bool tryRatsGetAts(Iso14443AInfo &info) {
  byte cmdRats[] = {0xE0, 0x80};   // RATS: FSDI=8, CID=0
  byte rx[PN532_MAX_ATS_LEN];
  byte rxLen = 0;

  if (!nfc.communicateThru(cmdRats, sizeof(cmdRats), rx, &rxLen, sizeof(rx))) {
    Serial.println("  RATS: error / no ATS");
    return false;
  }

  if (rxLen < 2) {
    Serial.println("  RATS: response too short");
    return false;
  }

  byte atsLen = rx[0];

  if (atsLen == 0 || atsLen > rxLen || atsLen > PN532_MAX_ATS_LEN) {
    Serial.println("  RATS: invalid ATS length");
    return false;
  }

  info.atsLength = atsLen;
  memcpy(info.ats, rx, atsLen);

  Serial.print("  RATS: ATS received: ");
  printHexBuf(info.ats, info.atsLength);
  Serial.println();

  return true;
}

void tryClassicOrPlusEv1Path(Iso14443AInfo &info, const char *classicFallback) {
  Serial.println("  Step: try RATS as shown in AN10833");

  if (tryRatsGetAts(info)) {
    Serial.println("  RATS result: ATS present");
    printAtsInfo(info);

    Serial.println("  Step: try GetVersion in Layer 4 wrapped APDU");

    VersionInfo vi2;
    memset(&vi2, 0, sizeof(vi2));

    if (getVersionL4Wrapped(vi2)) {
      printVersionConclusion(info, vi2);
    } else {
      Serial.println("  GetVersion L4 did not return product version");
      Serial.println("  Step: use ATS historical bytes");
      checkMifarePlusHistoricalBytes(info);
    }
  } else {
    Serial.print("  Flow result: ");
    Serial.println(classicFallback);
  }
}

// ---------- AN10833 flow ----------

void classifyByAn10833Flow(Iso14443AInfo &info, const VersionInfo &vi) {
  Serial.println();
  Serial.println("AN10833 flow decision:");

  Serial.print("  SAK bits:");
  Serial.print(" b1="); Serial.print(sakBit1(info.sak));
  Serial.print(" b2="); Serial.print(sakBit2(info.sak));
  Serial.print(" b4="); Serial.print(sakBit4(info.sak));
  Serial.print(" b5="); Serial.print(sakBit5(info.sak));
  Serial.print(" b6="); Serial.print(sakBit6(info.sak));
  Serial.print(" b7="); Serial.println(sakBit7(info.sak));

  if (sakBit2(info.sak)) {
    Serial.println("  Flow stop: SAK bit 2 is RFU in AN10833");
    return;
  }

  // ISO14443-4 branch.
  if (sakBit6(info.sak)) {
    Serial.println("  Branch: SAK b6=1 -> ISO14443-4 / RATS branch");

    printAtsInfo(info);

    Serial.println("  Step: GetVersion in Layer 4 wrapped APDU");

    if (vi.supported && vi.rawLength >= 7) {
      printVersionConclusion(info, vi);
    } else {
      Serial.println("  GetVersion L4 did not return product version");
      Serial.println("  Step: use ATS historical bytes where AN10833 gives a pattern");
      checkMifarePlusHistoricalBytes(info);
    }

    return;
  }

  // ISO14443-3 native branch.
  Serial.println("  Branch: SAK b6=0 -> ISO14443-3 native branch");

  if (!sakBit1(info.sak) && !sakBit4(info.sak) && !sakBit5(info.sak)) {
    Serial.println("  Sub-branch: b1=0, b4=0, b5=0");
    Serial.println("  Step: GetVersion in native Layer 3 path");

    if (vi.supported && vi.rawLength >= 7) {
      printVersionConclusion(info, vi);
    } else {
      Serial.println("  GetVersion native did not return product version");
      Serial.println("  Step: Ultralight-C AUTH part-1 probe");

      if (tryUltralightCAuthProbe()) {
        Serial.println("  Flow result: MIFARE Ultralight C");
      } else {
        Serial.println("  Flow result: MIFARE Ultralight");
      }
    }

    return;
  }

  if (sakBit4(info.sak) && !sakBit5(info.sak)) {
    Serial.println("  Sub-branch: b4=1, b5=0");

    if (sakBit1(info.sak)) {
      Serial.println("  Flow result: MIFARE Mini");
    } else {
      tryClassicOrPlusEv1Path(info, "MIFARE Classic 1K");
    }

    return;
  }

  if (sakBit4(info.sak) && sakBit5(info.sak)) {
    Serial.println("  Sub-branch: b4=1, b5=1");
    tryClassicOrPlusEv1Path(info, "MIFARE Classic 4K");
    return;
  }

  if (!sakBit4(info.sak) && sakBit5(info.sak)) {
    Serial.println("  Sub-branch: b4=0, b5=1");

    if (sakBit1(info.sak)) {
      Serial.println("  Flow result: MIFARE Plus 4K SL2");
    } else {
      Serial.println("  Flow result: MIFARE Plus 2K SL2");
    }

    return;
  }

  Serial.println("  Flow result: not decoded by current AN10833 subset");
}

// ---------- Gen1 probe ----------

bool tryPythonStyleGen1Probe() {
  Serial.println();
  Serial.println("Magic Gen1 probe:");
  Serial.println("Step: RF ON -> select card -> reset regs 6302/6303 -> HALT 50 00 -> 7-bit 40 -> 43");

  byte rx[32];
  byte rxLen = 0;

  Serial.println("Step: switch RF field ON");

  if (!nfc.switchOnRfField()) {
    Serial.println("Result: RF field ON failed");
    restoreMagicRegisters();
    return false;
  }

  delay(300);

  Iso14443AInfo info;

  Serial.println("Step: select ISO14443A card first");

  if (!nfc.readIso14443A(info) || !info.present) {
    Serial.println("Result: no ISO14443A card selected");
    restoreMagicRegisters();
    return false;
  }

  Serial.print("Selected UID: ");
  printHexBuf(info.uid, info.uidLength);
  Serial.println();

  byte resetRegs[] = {
    0x63, 0x02, 0x00,
    0x63, 0x03, 0x00
  };

  Serial.println("Step: WriteRegister 63 02 00 63 03 00");

  if (!nfc.writeRegisterBytes(resetRegs, sizeof(resetRegs))) {
    Serial.println("Result: reset register failed");
    restoreMagicRegisters();
    return false;
  }

  delay(50);

  byte haltCmd[] = {0x50, 0x00};
  rxLen = 0;

  Serial.println("Step: InCommunicateThru HALT 50 00");
  Serial.println("Note: timeout after HALT is expected");

  bool haltOk = nfc.communicateThru(haltCmd, sizeof(haltCmd), rx, &rxLen, sizeof(rx));

  Serial.print("HALT result: ");
  if (haltOk) {
    Serial.print("response ");
    printHexBuf(rx, rxLen);
    Serial.println();
  } else {
    Serial.println("timeout/error");
  }

  delay(100);

  byte bit7[] = {0x63, 0x3D, 0x07};

  Serial.println("Step: WriteRegister 63 3D 07");

  if (!nfc.writeRegisterBytes(bit7, sizeof(bit7))) {
    Serial.println("Result: set 7-bit frame failed");
    restoreMagicRegisters();
    return false;
  }

  delay(100);

  byte cmd40[] = {0x40};
  rxLen = 0;

  Serial.println("Step: InCommunicateThru 0x40 with BitFraming=7");

  bool ok40 = nfc.communicateThru(cmd40, sizeof(cmd40), rx, &rxLen, sizeof(rx));

  byte bit0[] = {0x63, 0x3D, 0x00};

  Serial.println("Step: WriteRegister 63 3D 00");
  nfc.writeRegisterBytes(bit0, sizeof(bit0));

  delay(100);

  if (!ok40 || rxLen < 1) {
    Serial.println("Result: no response after 0x40");
    restoreMagicRegisters();
    return false;
  }

  Serial.print("Response to 0x40: ");
  printHexBuf(rx, rxLen);
  Serial.println();

  if (rx[rxLen - 1] != 0x0A) {
    Serial.println("Result: bad ACK after 0x40");
    restoreMagicRegisters();
    return false;
  }

  Serial.println("ACK after 0x40 OK");

  delay(100);

  byte cmd43[] = {0x43};
  rxLen = 0;

  Serial.println("Step: InCommunicateThru 0x43");

  bool ok43 = nfc.communicateThru(cmd43, sizeof(cmd43), rx, &rxLen, sizeof(rx));

  if (!ok43 || rxLen < 1) {
    Serial.println("Result: no response after 0x43");
    restoreMagicRegisters();
    return false;
  }

  Serial.print("Response to 0x43: ");
  printHexBuf(rx, rxLen);
  Serial.println();

  if (rx[rxLen - 1] != 0x0A) {
    Serial.println("Result: bad ACK after 0x43");
    restoreMagicRegisters();
    return false;
  }

  Serial.println("Result: Gen1A/Gen1B magic UID backdoor present");

  restoreMagicRegisters();
  return true;
}

// ---------- Direct/no-auth probe ----------

bool tryDirectBlock0WriteProbe() {
  Serial.println();
  Serial.println("Direct block0 write probe:");
  Serial.println("Step: reselect card after Gen1 probe");
  Serial.println("Step: try raw MIFARE WRITE without auth");
  Serial.println("Note: writes same block 0 back unchanged");

  restoreMagicRegisters();

  nfc.ReleaseCard();
  nfc.SwitchOffRfField();
  delay(250);

  Iso14443AInfo tmp;
  if (!nfc.readIso14443A(tmp) || !tmp.present) {
    Serial.println("Result: card reselect failed before direct write test");
    return false;
  }

  Serial.print("Reselected UID: ");
  printHexBuf(tmp.uid, tmp.uidLength);
  Serial.println();

  byte readCmd[] = {0x30, 0x00};
  byte block0[18];
  byte rxLen = 0;

  if (!nfc.communicateThru(readCmd, sizeof(readCmd), block0, &rxLen, sizeof(block0))) {
    Serial.println("Result: cannot read block 0 without auth");
    return false;
  }

  if (rxLen < 16) {
    Serial.println("Result: short block 0 read");
    return false;
  }

  Serial.print("Original block0: ");
  printHexBuf(block0, 16);
  Serial.println();

  byte rx[8];

  byte writeCmd[] = {0xA0, 0x00};
  rxLen = 0;

  Serial.println("Step: send WRITE command A0 00");

  bool okCmd = nfc.communicateThru(writeCmd, sizeof(writeCmd), rx, &rxLen, sizeof(rx));

  if (!okCmd || !isAck0A(rx, rxLen)) {
    Serial.println("Result: WRITE command was not ACKed");
    return false;
  }

  Serial.println("WRITE command ACK OK");

  rxLen = 0;

  Serial.println("Step: send unchanged 16-byte block0 payload");

  bool okData = nfc.communicateThru(block0, 16, rx, &rxLen, sizeof(rx));

  if (!okData || !isAck0A(rx, rxLen)) {
    Serial.println("Result: WRITE data phase was not ACKed");
    return false;
  }

  Serial.println("WRITE data ACK OK");
  Serial.println("Result: direct block0 write accepted");

  return true;
}

// ---------- Gen2/CUID probe ----------

bool tryMagicGen2Block0WritableProbe(const Iso14443AInfo &info) {
  Serial.println();
  Serial.println("Magic Gen2/CUID probe:");
  Serial.println("Step: clean reselect before Gen2 auth");
  Serial.println("Step: authenticate sector 0 with default Key B");
  Serial.println("Step: read block 0");
  Serial.println("Step: write same block 0 back unchanged");

  restoreMagicRegisters();

  nfc.ReleaseCard();
  nfc.SwitchOffRfField();
  delay(250);

  Iso14443AInfo tmp;
  if (!nfc.readIso14443A(tmp) || !tmp.present) {
    Serial.println("Result: card reselect failed before Gen2 auth");
    return false;
  }

  Serial.print("Reselected UID before Gen2 auth: ");
  printHexBuf(tmp.uid, tmp.uidLength);
  Serial.println();

  const byte keyB_FF[6] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
  };

  Serial.println("Trying Key B FFFFFFFFFFFF on auth block 0");

  if (!nfc.classicAuthKeyB(0, tmp.uid, tmp.uidLength, keyB_FF)) {
    Serial.println("Result: Key B authentication failed on block 0");
    return false;
  }

  Serial.println("Auth OK with default Key B using block 0");

  byte block0[16];

  if (!nfc.classicReadBlock(0, block0)) {
    Serial.println("Result: read block 0 failed");
    return false;
  }

  Serial.print("Block 0 original: ");
  printHexBuf(block0, 16);
  Serial.println();

  Serial.println("Step: write block 0 back unchanged");

  if (!nfc.classicWriteBlockDebug(0, block0)) {
    Serial.println("Result: block 0 write rejected");
    Serial.println("Magic probe result: authenticated but block0 not writable");
    return false;
  }

  Serial.println("Result: block 0 write accepted");
  Serial.println("Magic probe result: Gen2/CUID-style UID block writable");

  return true;
}

bool tryMagicGen3SameUidWriteProbe(const Iso14443AInfo &info) {
  Serial.println();
  Serial.println("Magic Gen3 probe:");
  Serial.println("Step: clean reselect before Gen3 test");
  Serial.println("Step: send Gen3 UID-write command using same UID");
  Serial.println("Note: UID should remain unchanged");

  restoreMagicRegisters();

  nfc.ReleaseCard();
  nfc.SwitchOffRfField();
  delay(250);

  Iso14443AInfo tmp;
  if (!nfc.readIso14443A(tmp) || !tmp.present) {
    Serial.println("Result: card reselect failed before Gen3 test");
    return false;
  }

  Serial.print("Reselected UID: ");
  printHexBuf(tmp.uid, tmp.uidLength);
  Serial.println();

  if (tmp.uidLength != 7) {
    Serial.println("Result: Gen3 test only implemented for 7-byte UID cards");
    return false;
  }

  byte cmd[12];

  cmd[0] = 0x90;
  cmd[1] = 0xFB;
  cmd[2] = 0xCC;
  cmd[3] = 0xCC;
  cmd[4] = 0x07;

  memcpy(&cmd[5], tmp.uid, 7);

  Serial.print("Sending Gen3 same-UID command: ");
  printHexBuf(cmd, sizeof(cmd));
  Serial.println();

  byte rx[32];
  byte rxLen = 0;

  bool ok = nfc.communicateThru(
      cmd,
      sizeof(cmd),
      rx,
      &rxLen,
      sizeof(rx));

  if (!ok) {
    Serial.println("Result: no response to Gen3 command");
    return false;
  }

  Serial.print("Response: ");
  printHexBuf(rx, rxLen);
  Serial.println();

  delay(250);

  nfc.ReleaseCard();
  nfc.SwitchOffRfField();
  delay(250);

  Iso14443AInfo verify;
  if (!nfc.readIso14443A(verify) || !verify.present) {
    Serial.println("Result: card not found after Gen3 command");
    return false;
  }

  Serial.print("UID after command: ");
  printHexBuf(verify.uid, verify.uidLength);
  Serial.println();

  if (verify.uidLength != 7) {
    Serial.println("Result: unexpected UID length after command");
    return false;
  }

  if (memcmp(verify.uid, tmp.uid, 7) != 0) {
    Serial.println("Result: UID changed unexpectedly");
    return false;
  }

  Serial.println("Result: Gen3 same-UID command accepted");
  Serial.println("Magic probe result: Gen3 UID-changeable card");

  return true;
}

// ---------- capability-analysis gate ----------

bool shouldRunMagicAnalysis(const Iso14443AInfo &info) {
  // Never send Classic magic commands to ISO14443-4, Type 2, FeliCa,
  // or other unrelated card families.
  if (sakBit2(info.sak)) return false;
  if (sakIso14443_4(info.sak)) return false;

  // AN10833 Classic / Plus-looking branches use SAK bit 4 and/or bit 5.
  return sakBit4(info.sak) || sakBit5(info.sak);
}

// ---------- classifier ----------

const char *probeResultText(ProbeResult result) {
  switch (result) {
    case PROBE_MATCH:      return "YES";
    case PROBE_NO_MATCH:   return "No";
    case PROBE_NOT_TESTED: return "Not tested";
    default:               return "Unknown";
  }
}

MagicAnalysis analyzeMagicCard(Iso14443AInfo &info) {
  MagicAnalysis analysis;

  analysis.gen1 = PROBE_NOT_TESTED;
  analysis.direct = PROBE_NOT_TESTED;
  analysis.gen2 = PROBE_NOT_TESTED;
  analysis.gen3 = PROBE_NOT_TESTED;
  analysis.conclusion = MAGIC_NONE;

  Serial.println();
  Serial.println("Magic classifier:");
  Serial.println("Step: test Gen1 backdoor first");

  if (tryPythonStyleGen1Probe()) {
    analysis.gen1 = PROBE_MATCH;
    analysis.conclusion = MAGIC_GEN1;
    return analysis;
  }

  analysis.gen1 = PROBE_NO_MATCH;

  Serial.println();
  Serial.println("Step: test direct block0 write style");

  if (tryDirectBlock0WriteProbe()) {
    analysis.direct = PROBE_MATCH;
    analysis.conclusion = MAGIC_DIRECT;
    return analysis;
  }

  analysis.direct = PROBE_NO_MATCH;

  Serial.println();
  Serial.println("Step: test authenticated Gen2/CUID block-0 writable style");

  // The current merged PN532 Classic authentication helper accepts a
  // 4-byte Classic UID only. A 7-byte UID therefore means that this
  // specific Gen2 probe was not performed; it is not a negative result.
  if (info.uidLength == 4) {
    if (tryMagicGen2Block0WritableProbe(info)) {
      analysis.gen2 = PROBE_MATCH;
      analysis.conclusion = MAGIC_GEN2;
      return analysis;
    }

    analysis.gen2 = PROBE_NO_MATCH;
  } else {
    analysis.gen2 = PROBE_NOT_TESTED;
    Serial.print("Result: Gen2/CUID probe not tested for ");
    Serial.print(info.uidLength);
    Serial.println("-byte UID with the current Classic auth helper");
  }

  Serial.println();
  Serial.println("Step: test Gen3 same-UID write style");

  if (info.uidLength == 7) {
    if (tryMagicGen3SameUidWriteProbe(info)) {
      analysis.gen3 = PROBE_MATCH;
      analysis.conclusion = MAGIC_GEN3;
      return analysis;
    }

    analysis.gen3 = PROBE_NO_MATCH;
  } else {
    analysis.gen3 = PROBE_NOT_TESTED;
    Serial.println("Result: Gen3 probe applies only to 7-byte UID cards");
  }

  analysis.conclusion = MAGIC_NONE;
  return analysis;
}

void printMagicAnalysis(const MagicAnalysis &analysis, byte uidLength) {
  Serial.println();
  Serial.println("Magic capability analysis:");
  Serial.print("  Gen1 backdoor      : ");
  Serial.println(probeResultText(analysis.gen1));

  Serial.print("  Direct block0      : ");
  Serial.println(probeResultText(analysis.direct));

  Serial.print("  Gen2/CUID          : ");
  Serial.print(probeResultText(analysis.gen2));
  if (analysis.gen2 == PROBE_NOT_TESTED && uidLength != 4) {
    Serial.print(" (");
    Serial.print(uidLength);
    Serial.println("-byte UID not supported by current auth helper)");
  } else {
    Serial.println();
  }

  Serial.print("  Gen3               : ");
  Serial.print(probeResultText(analysis.gen3));
  if (analysis.gen3 == PROBE_NOT_TESTED && uidLength != 7) {
    Serial.println(" (requires a 7-byte UID)");
  } else {
    Serial.println();
  }

  Serial.println();
  Serial.println("Overall conclusion:");

  switch (analysis.conclusion) {
    case MAGIC_GEN1:
      Serial.println("  Gen1A/Gen1B UID-changeable magic card.");
      break;

    case MAGIC_DIRECT:
      Serial.println("  Direct/no-auth block0 writable magic card.");
      break;

    case MAGIC_GEN2:
      Serial.println("  Gen2/CUID-style block0 writable magic card.");
      break;

    case MAGIC_GEN3:
      Serial.println("  Gen3 7-byte UID-changeable magic card.");
      break;

    case MAGIC_NONE:
    default:
      Serial.println("  Not a supported magic card.");
      break;
  }
}

// ---------- reader ----------

void printReaderInfo() {
  byte IC, vh, vl, flags;

  Serial.println();
  Serial.println("Reader:");

  if (nfc.GetFirmwareVersion(&IC, &vh, &vl, &flags)) {
    Serial.print("  Chip: PN5");
    printHexByte(IC);
    Serial.println();

    Serial.print("  Firmware: ");
    Serial.print(vh);
    Serial.print('.');
    Serial.println(vl);
  } else {
    Serial.println("  GetFirmwareVersion failed");
  }
}

void identifyOnce() {
  Iso14443AInfo info;

  if (!nfc.readIso14443A(info)) {
    Serial.println("Card read error");
    nfc.SwitchOffRfField();
    delay(500);
    return;
  }

  if (!info.present) {
    lastUid = 0;

    static unsigned long lastFelicaPoll = 0;
    if (millis() - lastFelicaPoll < 500) return;
    lastFelicaPoll = millis();

    FelicaInfo fi;

    if (nfc.readFelica(fi, CARD_TYPE_FELICA_212KBPS) && fi.present) {
      printFelicaCard(fi);
      nfc.SwitchOffRfField();
      return;
    }

    if (nfc.readFelica(fi, CARD_TYPE_FELICA_424KBPS) && fi.present) {
      printFelicaCard(fi);
      nfc.SwitchOffRfField();
      return;
    }

    nfc.SwitchOffRfField();
    return;
  }

  uint64_t thisUid = uidTo64(info.uid, info.uidLength);
  if (thisUid == lastUid) return;
  lastUid = thisUid;

  Serial.println();
  Serial.println("================ CARD ================");

  Serial.print("UID: ");
  printHexBuf(info.uid, info.uidLength);
  Serial.println();

  Serial.print("UID length: ");
  Serial.println(info.uidLength);

  Serial.print("ATQA/SENS_RES: 0x");
  printHexByte(info.atqa >> 8);
  printHexByte(info.atqa & 0xFF);
  Serial.println();

  Serial.print("SAK/SEL_RES: 0x");
  printHexByte(info.sak);
  Serial.println();

  VersionInfo vi;
  memset(&vi, 0, sizeof(vi));

  // Only try GetVersion where the AN10833 flow needs it before classification.
  // Classic-looking branches do their own RATS -> L4 GetVersion test inside the flow.
  if (sakIso14443_4(info.sak)) {
    getVersionL4Wrapped(vi);
  } else {
    if (!sakBit1(info.sak) &&
        !sakBit2(info.sak) &&
        !sakBit4(info.sak) &&
        !sakBit5(info.sak)) {
      rawGetVersion(vi);
    }
  }

  classifyByAn10833Flow(info, vi);

  Serial.println();
  Serial.println("Capability analyzer:");

  if (shouldRunMagicAnalysis(info)) {
    MagicAnalysis magicAnalysis = analyzeMagicCard(info);
    printMagicAnalysis(magicAnalysis, info.uidLength);
  } else {
    Serial.println("  Magic probes skipped.");
    Serial.println("  Reason: card is not on a Classic/Plus-compatible AN10833 branch.");
  }

  Serial.println("======================================");

  nfc.ReleaseCard();
  nfc.SwitchOffRfField();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.print("FW: ");
  const char* p = strrchr(__FILE__, '/');
  const char* q = strrchr(__FILE__, '\\');
  Serial.print((p > q ? p : q) ? (p > q ? p : q) + 1 : __FILE__);

  Serial.print(" | ");
  Serial.print(__DATE__);
  Serial.print(" ");
  Serial.println(__TIME__);
  Serial.println("PN532 Card Identifier + Capability Analyzer V002");
  Serial.println("ESP32 SPI: SCK=18 MISO=19 MOSI=23 CS=5");
  Serial.println("Identification runs first; applicable magic probes run second.");
  Serial.println("Direct/Gen2 probes rewrite unchanged block 0; Gen3 rewrites the same UID.");
  Serial.println("Present one card at a time.");

  nfc.InitHardwareSPI(PN532_CS_PIN, PN532_RESET_PIN);
  nfc.begin();

  printReaderInfo();

  if (!nfc.SetPassiveActivationRetries()) Serial.println("SetPassiveActivationRetries failed");
  if (!nfc.SamConfig()) Serial.println("SAMConfig failed");

  Serial.println();
  Serial.println("Present a card...");
}

void loop() {
  identifyOnce();
  delay(100);
}
