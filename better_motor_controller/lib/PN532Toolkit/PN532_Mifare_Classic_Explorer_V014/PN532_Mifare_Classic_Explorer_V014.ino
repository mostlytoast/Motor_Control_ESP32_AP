/*
  PN532 MIFARE Classic 1K Serial Shell V001
  ESP32 / Hardware SPI
  Wiring: SCK=18, MISO=19, MOSI=23, CS=5, RST not connected.

  Uses your PN532.h / Utils.h library style.

  Safe functions:
    - read UID / ATQA / SAK
    - read blocks with known keys
    - write normal data blocks only
    - auto-map sectors using known key list only
    - value block detection

  Refuses writes to:
    - block 0
    - trailer blocks 3,7,11,...,63
*/

#include "Utils.h"
#include "PN532.h"

#define PN532_CS_PIN     5
#define PN532_RESET_PIN  255

#define MAX_UID_LEN      10
#define MAX_KEYS         24
bool pendingAccess = false;
byte pendingSector = 0;
byte pendingTrailer[16];

byte trailerBlockOfSector(byte sector);
byte sectorOfBlock(byte block);
bool isTrailer(byte block);
bool readBlockMapped(byte block, byte *data);
bool authBlockMapped(byte block);
void buildAccessBytes(byte c[4][3], byte &b6, byte &b7, byte &b8);
void printDataBlockLine(byte block, byte *data, byte *trailer);
void printTrailerLine(byte block, byte *data, byte sector);

struct SectorKey {
  bool known;
  bool isKeyB;
  byte key[6];
};

class ClassicPN532 : public PN532 {
public:
  bool readIso14443A(Iso14443AInfo &info) {
    memset(&info, 0, sizeof(info));

    mu8_PacketBuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
    mu8_PacketBuffer[1] = 1;
    mu8_PacketBuffer[2] = CARD_TYPE_106KB_ISO14443A;

    if (!SendCommandCheckAck(mu8_PacketBuffer, 3)) return false;

    byte len = ReadData(mu8_PacketBuffer, 64);
    if (len < 3 || mu8_PacketBuffer[1] != PN532_COMMAND_INLISTPASSIVETARGET + 1) {
      return false;
    }

    if (mu8_PacketBuffer[2] != 1) {
      info.present = false;
      return true;
    }

    info.present = true;
    info.atqa = ((uint16_t)mu8_PacketBuffer[4] << 8) | mu8_PacketBuffer[5];
    info.sak = mu8_PacketBuffer[6];
    info.uidLength = mu8_PacketBuffer[7];

    if (info.uidLength > MAX_UID_LEN) info.uidLength = MAX_UID_LEN;
    memcpy(info.uid, mu8_PacketBuffer + 8, info.uidLength);

    return true;
  }

  bool dataExchange(const byte *tx, byte txLen, byte *rx, byte *rxLen, byte rxMax) {
    if (txLen + 2 > PN532_PACKBUFFSIZE) return false;

    mu8_PacketBuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
    mu8_PacketBuffer[1] = 1;
    memcpy(mu8_PacketBuffer + 2, tx, txLen);

    if (!SendCommandCheckAck(mu8_PacketBuffer, txLen + 2)) return false;

    byte len = ReadData(mu8_PacketBuffer, rxMax + 8);
    if (len < 3 || mu8_PacketBuffer[1] != PN532_COMMAND_INDATAEXCHANGE + 1) return false;
    if (!CheckPN532Status(mu8_PacketBuffer[2])) return false;

    byte dataLen = len - 3;
    if (dataLen > rxMax) dataLen = rxMax;

    memcpy(rx, mu8_PacketBuffer + 3, dataLen);
    *rxLen = dataLen;
    return true;
  }

  bool classicAuth(byte block, const byte *uid, byte uidLength, const byte *key, bool keyB) {
    if (uidLength != 4) {
      Serial.println("Classic auth needs 4-byte UID");
      return false;
    }

    byte cmd[12];
    cmd[0] = keyB ? 0x61 : 0x60;
    cmd[1] = block;
    memcpy(cmd + 2, key, 6);
    memcpy(cmd + 8, uid, 4);

    byte rx[8];
    byte rxLen = 0;

    return dataExchange(cmd, sizeof(cmd), rx, &rxLen, sizeof(rx));
  }

  bool classicReadBlock(byte block, byte *data16) {
    byte cmd[] = {0x30, block};
    byte rx[24];
    byte rxLen = 0;

    if (!dataExchange(cmd, sizeof(cmd), rx, &rxLen, sizeof(rx))) return false;
    if (rxLen < 16) return false;

    memcpy(data16, rx, 16);
    return true;
  }

  bool classicWriteBlock(byte block, const byte *data16) {
    byte cmd[18];
    cmd[0] = 0xA0;
    cmd[1] = block;
    memcpy(cmd + 2, data16, 16);

    byte rx[8];
    byte rxLen = 0;

    return dataExchange(cmd, sizeof(cmd), rx, &rxLen, sizeof(rx));
  }
};

ClassicPN532 nfc;

void SetSuppressAuthErrors(bool suppress)
{
    (void)suppress;
}

Iso14443AInfo currentCard;

byte keyA[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
byte keyB[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
bool useKeyB = false;

byte knownKeys[MAX_KEYS][6];
int keyCount = 0;

SectorKey sectorKeys[16];

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

void printKey(const byte *k) {
  for (byte i = 0; i < 6; i++) printHexByte(k[i]);
}

void printBlock(byte block, byte *data) {
  Serial.print("Block ");
  if (block < 10) Serial.print(' ');
  Serial.print(block);
  Serial.print(": ");

  printHexBuf(data, 16);

  Serial.print("  | ");
  for (byte i = 0; i < 16; i++) {
    char c = data[i];
    Serial.print((c >= 32 && c <= 126) ? c : '.');
  }

  if ((block % 4) == 3) Serial.print("  <TRAILER>");
  Serial.println();
}

// ---------- hex parser ----------

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool parseHexBytes(String s, byte *out, int maxLen, int &outLen) {
  s.replace(" ", "");
  s.replace(":", "");
  s.replace("-", "");

  if (s.length() % 2) return false;

  outLen = s.length() / 2;
  if (outLen > maxLen) return false;

  for (int i = 0; i < outLen; i++) {
    int hi = hexNibble(s[i * 2]);
    int lo = hexNibble(s[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (hi << 4) | lo;
  }

  return true;
}

// ---------- card helpers ----------

bool selectCard() {
  memset(&currentCard, 0, sizeof(currentCard));

  if (!nfc.readIso14443A(currentCard)) {
    Serial.println("Card read error");
    return false;
  }

  if (!currentCard.present) {
    Serial.println("No card");
    return false;
  }

  return true;
}

void getBlockAccessText(byte *trailer, byte block, char *out) {
  byte local = block % 4;   // 0..3
  byte b6 = trailer[6];
  byte b7 = trailer[7];
  byte b8 = trailer[8];

  byte c1 = (b7 >> (4 + local)) & 1;
  byte c2 = (b8 >> local) & 1;
  byte c3 = (b8 >> (4 + local)) & 1;

  byte c = (c1 << 2) | (c2 << 1) | c3;

  // Only for DATA blocks (0..2)
  switch (c) {
    case 0b000:
      strcpy(out, "R/W keyA/B");
      break;

    case 0b010:
      strcpy(out, "R keyA/B");
      break;

    case 0b100:
      strcpy(out, "R keyA/B  W keyB");
      break;

    default:
      strcpy(out, "Access custom");
      break;
  }
}

void printDataBlockLine(byte block, byte *data, byte *trailer) {
  Serial.print("Block ");
  if (block < 10) Serial.print(' ');
  Serial.print(block);
  Serial.print(": ");

  printHexBuf(data, 16);

  Serial.print("  | ");

  for (byte i = 0; i < 16; i++) {
    char c = data[i];
    Serial.print((c >= 32 && c <= 126) ? c : '.');
  }

  char access[32];
  getBlockAccessText(trailer, block, access);

  Serial.print(" ");
  Serial.print(access);

  Serial.println();
}

void printTrailerLine(byte block, byte *data, byte sector) {
  Serial.print("Block ");
  if (block < 10) Serial.print(' ');
  Serial.print(block);
  Serial.print(": ");

  // Key A: card returns 00s, so show known mapped key or XX
  if (sectorKeys[sector].known && !sectorKeys[sector].isKeyB) {
    printHexBuf(sectorKeys[sector].key, 6);
  } else {
    Serial.print("XX XX XX XX XX XX");
  }

  Serial.print(" ");

  // Access bytes + GP byte
  printHexBuf(data + 6, 4);
  Serial.print(" ");

  // Key B as read from card
  printHexBuf(data + 10, 6);

  Serial.print("  | ");

  for (byte i = 0; i < 16; i++) {
    char c = data[i];
    Serial.print((c >= 32 && c <= 126) ? c : '.');
  }

  Serial.print(" TRAILER ");

if (data[6] == 0xFF && data[7] == 0x07 && data[8] == 0x80) {
  Serial.print("Access: open");
} else if (data[6] == 0x8F && data[7] == 0x07 && data[8] == 0x87) {
  Serial.print("Access: readonly");
} else if (data[6] == 0xF8 && data[7] == 0x77 && data[8] == 0x80) {
  Serial.print("Access: keyB_write");
} else {
  Serial.print("Access: custom");
}

  Serial.println();
}

void cmdSetAccessPreview(byte sector, String mode) {
  if (sector > 15) {
    Serial.println("Sector out of range");
    return;
  }

  mode.toLowerCase();

  byte trailer = trailerBlockOfSector(sector);
  byte oldT[16];

  if (!readBlockMapped(trailer, oldT)) {
    Serial.println("Could not read trailer");
    return;
  }

  memcpy(pendingTrailer, oldT, 16);
  pendingSector = sector;
  if (sectorKeys[sector].known && !sectorKeys[sector].isKeyB) {
  memcpy(pendingTrailer, sectorKeys[sector].key, 6);
} else {
  Serial.println("REFUSE: Key A unknown, cannot safely modify access bits");
  pendingAccess = false;
  return;
}

  byte c[4][3];

  if (mode == "open") {
    // transport config: data blocks 000, trailer 001
    byte b6 = 0xFF, b7 = 0x07, b8 = 0x80;
    pendingTrailer[6] = b6;
    pendingTrailer[7] = b7;
    pendingTrailer[8] = b8;
    // preserve byte 9
  }
  else if (mode == "readonly") {
    // data blocks 010: read A/B, write never
    // trailer 001: keep keys/access writable in normal transport-like way
    for (byte i = 0; i < 3; i++) {
      c[i][0] = 0;
      c[i][1] = 1;
      c[i][2] = 0;
    }
    c[3][0] = 0;
    c[3][1] = 0;
    c[3][2] = 1;

    buildAccessBytes(c, pendingTrailer[6], pendingTrailer[7], pendingTrailer[8]);
  }
  else if (mode == "keyb_write") {
    // data blocks 100: read A/B, write Key B only
    // trailer 001
    for (byte i = 0; i < 3; i++) {
      c[i][0] = 1;
      c[i][1] = 0;
      c[i][2] = 0;
    }
    c[3][0] = 0;
    c[3][1] = 0;
    c[3][2] = 1;

    buildAccessBytes(c, pendingTrailer[6], pendingTrailer[7], pendingTrailer[8]);
  }
  else {
    Serial.println("Usage: setaccess <sector> open|readonly|keyb_write");
    return;
  }

  pendingAccess = true;

  Serial.println();
  Serial.println("ACCESS CHANGE PREVIEW");
  Serial.print("Sector: "); Serial.println(sector);
  Serial.print("Trailer block: "); Serial.println(trailer);
  Serial.print("Mode: "); Serial.println(mode);
  Serial.println();

  Serial.println("Old trailer:");
  printBlock(trailer, oldT);

  Serial.println("New trailer:");
  printBlock(trailer, pendingTrailer);

  Serial.println();
  Serial.println("Changed bytes:");
  Serial.print("  byte 6: "); printHexByte(oldT[6]); Serial.print(" -> "); printHexByte(pendingTrailer[6]); Serial.println();
  Serial.print("  byte 7: "); printHexByte(oldT[7]); Serial.print(" -> "); printHexByte(pendingTrailer[7]); Serial.println();
  Serial.print("  byte 8: "); printHexByte(oldT[8]); Serial.print(" -> "); printHexByte(pendingTrailer[8]); Serial.println();
  Serial.print("  byte 9 GP preserved: "); printHexByte(oldT[9]); Serial.println();

  Serial.println();
  Serial.println("Type CONFIRM to write, or cancel to abort.");
}

void cmdConfirmAccess() {
  if (!pendingAccess) {
    Serial.println("No pending access change");
    return;
  }

  byte trailer = trailerBlockOfSector(pendingSector);

  if (!authBlockMapped(trailer)) {
    Serial.println("AUTH FAIL");
    return;
  }

  if (!nfc.classicWriteBlock(trailer, pendingTrailer)) {
    Serial.println("ACCESS WRITE FAIL");
    return;
  }

  Serial.println("ACCESS WRITE OK");
  printTrailerLine(trailer, pendingTrailer, pendingSector);

  // Keep known Key A cached after access write
  sectorKeys[pendingSector].known = true;
  sectorKeys[pendingSector].isKeyB = false;
  memcpy(sectorKeys[pendingSector].key, pendingTrailer, 6);

  pendingAccess = false;
}

void cmdCancelAccess() {
  pendingAccess = false;
  Serial.println("Pending access change cancelled");
}

void buildAccessBytes(byte c[4][3], byte &b6, byte &b7, byte &b8) {
  byte c1 = 0, c2 = 0, c3 = 0;

  for (byte i = 0; i < 4; i++) {
    if (c[i][0]) c1 |= (1 << i);
    if (c[i][1]) c2 |= (1 << i);
    if (c[i][2]) c3 |= (1 << i);
  }

  b6 = ((~c2 & 0x0F) << 4) | (~c1 & 0x0F);
  b7 = (( c1 & 0x0F) << 4) | (~c3 & 0x0F);
  b8 = (( c3 & 0x0F) << 4) | ( c2 & 0x0F);
}

byte sectorOfBlock(byte block) {
  return block / 4;
}

byte trailerBlockOfSector(byte sector) {
  return sector * 4 + 3;
}

bool isTrailer(byte block) {
  return (block % 4) == 3;
}

byte *currentManualKey() {
  return useKeyB ? keyB : keyA;
}

// ---------- auth / read / write ----------

bool authBlockWith(byte block, bool keyBMode, byte *key) {
  if (!selectCard()) return false;
  return nfc.classicAuth(block, currentCard.uid, currentCard.uidLength, key, keyBMode);
}

bool authBlockMapped(byte block) {
  byte sector = sectorOfBlock(block);

  if (sectorKeys[sector].known) {
    return authBlockWith(block, sectorKeys[sector].isKeyB, sectorKeys[sector].key);
  }

  return authBlockWith(block, useKeyB, currentManualKey());
}

bool readBlockMapped(byte block, byte *data) {
  if (block > 63) {
    Serial.println("Block out of range");
    return false;
  }

  if (!authBlockMapped(block)) {
    Serial.print("Block ");
    Serial.print(block);
    Serial.println(" AUTH FAIL");
    return false;
  }

  if (!nfc.classicReadBlock(block, data)) {
    Serial.print("Block ");
    Serial.print(block);
    Serial.println(" READ FAIL");
    return false;
  }

  return true;
}

// ---------- commands ----------

void cmdInfo() {
  if (!selectCard()) return;

  Serial.println();
  Serial.println("================ CARD ================");

  Serial.print("UID: ");
  printHexBuf(currentCard.uid, currentCard.uidLength);
  Serial.println();

  Serial.print("UID length: ");
  Serial.println(currentCard.uidLength);

  Serial.print("ATQA/SENS_RES: 0x");
  printHexByte(currentCard.atqa >> 8);
  printHexByte(currentCard.atqa & 0xFF);
  Serial.println();

  Serial.print("SAK/SEL_RES: 0x");
  printHexByte(currentCard.sak);
  Serial.println();

  if (currentCard.sak == 0x08) Serial.println("Guess: MIFARE Classic 1K");
  else if (currentCard.sak == 0x18) Serial.println("Guess: MIFARE Classic 4K");
  else Serial.println("Guess: not plain Classic 1K/4K or unknown");

  Serial.println("======================================");
}

void cmdReadHex(byte block) {
  byte data[16];
  if (readBlockMapped(block, data)) printBlock(block, data);
}

void cmdReadAscii(byte block) {
  byte data[16];
  if (!readBlockMapped(block, data)) return;

  Serial.print("ASCII ");
  Serial.print(block);
  Serial.print(": ");

  for (byte i = 0; i < 16; i++) {
    char c = data[i];
    Serial.print((c >= 32 && c <= 126) ? c : '.');
  }

  Serial.println();
}

void cmdWritePartial(byte block, String hex) {
  if (block > 63) {
    Serial.println("Block out of range");
    return;
  }

  if (block == 0 || isTrailer(block)) {
    Serial.println("Refusing to write manufacturer/trailer block");
    return;
  }

  byte patch[16];
  int patchLen = 0;

  if (!parseHexBytes(hex, patch, 16, patchLen)) {
    Serial.println("Bad hex");
    return;
  }

  byte data[16];

  if (!readBlockMapped(block, data)) {
    Serial.println("Read-before-write failed");
    return;
  }

  for (int i = 0; i < patchLen; i++) data[i] = patch[i];

  if (!authBlockMapped(block)) {
    Serial.println("AUTH FAIL before write");
    return;
  }

  if (!nfc.classicWriteBlock(block, data)) {
    Serial.println("WRITE FAIL");
    return;
  }

  Serial.println("WRITE OK");
  printBlock(block, data);
}

void cmdWriteFull(byte block, String hex) {
  if (block > 63) {
    Serial.println("Block out of range");
    return;
  }

  if (block == 0 || isTrailer(block)) {
    Serial.println("Refusing to write manufacturer/trailer block");
    return;
  }

  byte data[16];
  int len = 0;

  if (!parseHexBytes(hex, data, 16, len) || len != 16) {
    Serial.println("Need exactly 16 bytes / 32 hex chars");
    return;
  }

  if (!authBlockMapped(block)) {
    Serial.println("AUTH FAIL");
    return;
  }

  if (!nfc.classicWriteBlock(block, data)) {
    Serial.println("WRITE FAIL");
    return;
  }

  Serial.println("WRITE OK");
  printBlock(block, data);
}

// ---------- known keys ----------

void addKnownKeyBytes(const byte *k) {
  if (keyCount >= MAX_KEYS) {
    Serial.println("Known key list full");
    return;
  }

  memcpy(knownKeys[keyCount], k, 6);
  keyCount++;
}

void addKnownKeyHex(String hex) {
  byte k[6];
  int len = 0;

  if (!parseHexBytes(hex, k, 6, len) || len != 6) {
    Serial.println("Key must be 12 hex chars");
    return;
  }

  addKnownKeyBytes(k);

  Serial.print("Added key ");
  printKey(k);
  Serial.println();
}

void loadDefaultKeys() {
  keyCount = 0;

  byte k1[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  byte k2[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
  byte k3[6] = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
  byte k4[6] = {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5};
  byte k5[6] = {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7};
  byte k6[6] = {0x4D,0x3A,0x99,0xC3,0x51,0xDD};
  byte k7[6] = {0x1A,0x98,0x2C,0x7E,0x45,0x9A};

  addKnownKeyBytes(k1);
  addKnownKeyBytes(k2);
  addKnownKeyBytes(k3);
  addKnownKeyBytes(k4);
  addKnownKeyBytes(k5);
  addKnownKeyBytes(k6);
  addKnownKeyBytes(k7);
}

void cmdKeys() {
  Serial.println();
  Serial.println("Known keys:");

  for (int i = 0; i < keyCount; i++) {
    Serial.print("  ");
    Serial.print(i);
    Serial.print(": ");
    printKey(knownKeys[i]);
    Serial.println();
  }

  Serial.print("Manual Key A: ");
  printKey(keyA);
  Serial.println();

  Serial.print("Manual Key B: ");
  printKey(keyB);
  Serial.println();

  Serial.print("Using manual Key ");
  Serial.println(useKeyB ? "B" : "A");
}

void setManualKey(bool setB, String hex) {
  byte k[6];
  int len = 0;

  if (!parseHexBytes(hex, k, 6, len) || len != 6) {
    Serial.println("Key must be 12 hex chars");
    return;
  }

  if (setB) memcpy(keyB, k, 6);
  else memcpy(keyA, k, 6);

  Serial.print("Set Key ");
  Serial.print(setB ? "B: " : "A: ");
  printKey(k);
  Serial.println();
}

// ---------- sector map ----------

void clearSectorMap() {
  for (byte i = 0; i < 16; i++) {
    sectorKeys[i].known = false;
    sectorKeys[i].isKeyB = false;
    memset(sectorKeys[i].key, 0, 6);
  }
}

bool trySectorKey(byte sector) {
  if (sector > 15) {
    Serial.println("Sector out of range");
    return false;
  }

  byte trailer = trailerBlockOfSector(sector);

  // First try all keys as Key A
  for (int k = 0; k < keyCount; k++) {
    if (authBlockWith(trailer, false, knownKeys[k])) {
      sectorKeys[sector].known = true;
      sectorKeys[sector].isKeyB = false;
      memcpy(sectorKeys[sector].key, knownKeys[k], 6);

      nfc.ReleaseCard();
      return true;
    }

    nfc.ReleaseCard();
    delay(40);
  }

  // Then try all keys as Key B
  for (int k = 0; k < keyCount; k++) {
    if (authBlockWith(trailer, true, knownKeys[k])) {
      sectorKeys[sector].known = true;
      sectorKeys[sector].isKeyB = true;
      memcpy(sectorKeys[sector].key, knownKeys[k], 6);

      nfc.ReleaseCard();
      return true;
    }

    nfc.ReleaseCard();
    delay(40);
  }

  Serial.print("Sector ");
  Serial.print(sector);
  Serial.println(" no known key");
  return false;
}

void cmdAutoMap() {
  clearSectorMap();

  Serial.println("Auto-mapping sectors using known key list...");

  SetSuppressAuthErrors(true);

  for (byte s = 0; s < 16; s++) {
    trySectorKey(s);
  }

  SetSuppressAuthErrors(false);

  Serial.println();
  Serial.println("Sector map:");

  for (byte s = 0; s < 16; s++) {
    Serial.print("  Sector ");
    Serial.print(s);
    Serial.print(": ");

    if (!sectorKeys[s].known) {
      Serial.println("unknown");
    } else {
      Serial.print("Key ");
      Serial.print(sectorKeys[s].isKeyB ? "B " : "A ");
      printKey(sectorKeys[s].key);
      Serial.println();
    }
  }
}


// ---------- dump ----------

void cmdDumpSector(byte sector) {
  if (sector > 15) {
    Serial.println("Sector out of range");
    return;
  }

  byte trailer = trailerBlockOfSector(sector);
  byte start   = sector * 4;

  Serial.println();
  Serial.print("Sector ");
  Serial.println(sector);
  if (!sectorKeys[sector].known) {
  Serial.println("No cached key for this sector -> running automap first...");
  cmdAutoMap();

  if (!sectorKeys[sector].known) {
    Serial.println("AUTH FAIL - no known key for this sector");
    return;
  }
}

  bool ok;

  if (sectorKeys[sector].known) {
    ok = authBlockWith(
      trailer,
      sectorKeys[sector].isKeyB,
      sectorKeys[sector].key
    );
  } else {
    ok = authBlockWith(
      trailer,
      useKeyB,
      currentManualKey()
    );
  }

  if (!ok) {
    Serial.println("AUTH FAIL");
    nfc.ReleaseCard();
    return;
  }

  // Read trailer first so we can decode access bits for blocks 0..2
  byte trailerData[16];

  if (!nfc.classicReadBlock(trailer, trailerData)) {
    Serial.println("Trailer read failed");
    nfc.ReleaseCard();
    return;
  }

  for (byte b = start; b < start + 4; b++) {
    byte data[16];

    // We already have trailer data
    if (b == trailer) {
      memcpy(data, trailerData, 16);
    } else {
      if (!nfc.classicReadBlock(b, data)) {
        Serial.print("Block ");
        Serial.print(b);
        Serial.println(" READ FAIL");
        continue;
      }
    }

    if (isTrailer(b)) {
      printTrailerLine(b, data, sector);
    } else {
      printDataBlockLine(b, data, trailerData);
    }
  }

  nfc.ReleaseCard();
}

void cmdInstantDump() {
  Serial.println();
  Serial.println("Dump using cached sector map only");
  Serial.println("Run automap first if sectors are unknown.");

  if (!selectCard()) return;

  for (byte sector = 0; sector < 16; sector++) {
    byte trailer = trailerBlockOfSector(sector);
    byte start   = sector * 4;

    Serial.println();
    Serial.print("Sector ");
    Serial.println(sector);

    if (!sectorKeys[sector].known) {
      Serial.println("SKIP - no cached key");
      continue;
    }

    bool ok = nfc.classicAuth(
      trailer,
      currentCard.uid,
      currentCard.uidLength,
      sectorKeys[sector].key,
      sectorKeys[sector].isKeyB
    );

    if (!ok) {
      Serial.println("AUTH FAIL");
      continue;
    }

    byte trailerData[16];

    if (!nfc.classicReadBlock(trailer, trailerData)) {
      Serial.println("Trailer read failed");
      continue;
    }

    for (byte b = start; b < start + 4; b++) {
      byte data[16];

      if (b == trailer) {
        memcpy(data, trailerData, 16);
      } else {
        if (!nfc.classicReadBlock(b, data)) {
          Serial.print("Block ");
          Serial.print(b);
          Serial.println(" READ FAIL");
          continue;
        }
      }

      if (isTrailer(b)) {
        printTrailerLine(b, data, sector);
      } else {
        printDataBlockLine(b, data, trailerData);
      }
    }
  }

  nfc.ReleaseCard();
}

void cmdDumpAll() {
  for (byte s = 0; s < 16; s++) {
    cmdDumpSector(s);
  }
}

// ---------- access bits ----------

void decodeAccessBits(byte *t) {
  byte b6 = t[6];
  byte b7 = t[7];
  byte b8 = t[8];

  Serial.println("Access bytes:");
  Serial.print("  byte6 = 0x"); printHexByte(b6); Serial.println();
  Serial.print("  byte7 = 0x"); printHexByte(b7); Serial.println();
  Serial.print("  byte8 = 0x"); printHexByte(b8); Serial.println();

  Serial.println("Raw C bits per local block:");

  for (byte local = 0; local < 4; local++) {
    byte c1 = (b7 >> (4 + local)) & 1;
    byte c2 = (b8 >> local) & 1;
    byte c3 = (b8 >> (4 + local)) & 1;

    Serial.print("  local block ");
    Serial.print(local);
    Serial.print(": C1=");
    Serial.print(c1);
    Serial.print(" C2=");
    Serial.print(c2);
    Serial.print(" C3=");
    Serial.println(c3);
  }

  bool invOk =
    (((b6 >> 4) & 0x0F) == ((~b8) & 0x0F)) &&
    ((b6 & 0x0F) == (((~b7) >> 4) & 0x0F)) &&
    ((b7 & 0x0F) == (((~b8) >> 4) & 0x0F));

  Serial.print("Inverted access check: ");
  Serial.println(invOk ? "OK" : "BAD / unusual");
}

void cmdTrailer(byte sector) {
  if (sector > 15) {
    Serial.println("Sector out of range");
    return;
  }

  cmdReadHex(trailerBlockOfSector(sector));
}

void cmdAccess(byte sector) {
  if (sector > 15) {
    Serial.println("Sector out of range");
    return;
  }

  byte data[16];
  byte trailer = trailerBlockOfSector(sector);

  if (!readBlockMapped(trailer, data)) return;

  printBlock(trailer, data);
  decodeAccessBits(data);
}

// ---------- value block ----------

bool isValueBlock(byte *d) {
  uint32_t v1 =
    ((uint32_t)d[0]) |
    ((uint32_t)d[1] << 8) |
    ((uint32_t)d[2] << 16) |
    ((uint32_t)d[3] << 24);

  uint32_t nv =
    ((uint32_t)d[4]) |
    ((uint32_t)d[5] << 8) |
    ((uint32_t)d[6] << 16) |
    ((uint32_t)d[7] << 24);

  uint32_t v2 =
    ((uint32_t)d[8]) |
    ((uint32_t)d[9] << 8) |
    ((uint32_t)d[10] << 16) |
    ((uint32_t)d[11] << 24);

  bool valueOk = (v1 == v2) && (nv == ~v1);

  bool addrOk =
    d[12] == (byte)~d[13] &&
    d[12] == d[14] &&
    d[13] == d[15];

  return valueOk && addrOk;
}
void cmdWriteKey(byte sector, bool writeB, String hex) {
  if (sector > 15) {
    Serial.println("Sector out of range");
    return;
  }

  byte newKey[6];
  int len = 0;

  if (!parseHexBytes(hex, newKey, 6, len) || len != 6) {
    Serial.println("Key must be exactly 6 bytes / 12 hex chars");
    return;
  }

  byte trailer = trailerBlockOfSector(sector);
  byte data[16];

  if (!readBlockMapped(trailer, data)) {
    Serial.println("Could not read trailer");
    return;
  }

  if (writeB) {
    // Preserve hidden Key A. Card read returns 00s for Key A.
    if (sectorKeys[sector].known && !sectorKeys[sector].isKeyB) {
      memcpy(data, sectorKeys[sector].key, 6);
    } else {
      Serial.println("REFUSE: Key A unknown, cannot safely write Key B");
      return;
    }

    memcpy(data + 10, newKey, 6);   // Key B
  } else {
    memcpy(data, newKey, 6);        // Key A
  }

  if (!authBlockMapped(trailer)) {
    Serial.println("AUTH FAIL before trailer write");
    return;
  }

  if (!nfc.classicWriteBlock(trailer, data)) {
    Serial.println("KEY WRITE FAIL");
    return;
  }

  Serial.print("Key ");
  Serial.print(writeB ? "B" : "A");
  Serial.print(" updated for sector ");
  Serial.println(sector);

  printBlock(trailer, data);

  sectorKeys[sector].known = true;
  sectorKeys[sector].isKeyB = writeB;
  memcpy(sectorKeys[sector].key, newKey, 6);
}

void cmdValue(byte block) {
  if (block > 63) {
    Serial.println("Block out of range");
    return;
  }

  byte d[16];
  if (!readBlockMapped(block, d)) return;

  printBlock(block, d);

  if (!isValueBlock(d)) {
    Serial.println("Not a valid Classic value block");
    return;
  }

  int32_t value =
    ((uint32_t)d[0]) |
    ((uint32_t)d[1] << 8) |
    ((uint32_t)d[2] << 16) |
    ((uint32_t)d[3] << 24);

  Serial.print("Value block OK, value = ");
  Serial.println(value);

  Serial.print("Address byte = 0x");
  printHexByte(d[12]);
  Serial.println();
}

// ---------- parser ----------

void printHelp() {
  Serial.println();
  Serial.println("MIFARE Classic 1K serial shell");
  Serial.println();

  auto row = [](const char* l, const char* r) {
    Serial.print("  ");
    Serial.print(l);

    int pad = 34 - strlen(l);
    if (pad < 1) pad = 1;
    for (int i = 0; i < pad; i++) Serial.print(' ');

    Serial.println(r);
  };

  Serial.println("Card / Keys:");
  row("info",              "keys");
  row("addkey AABBCCDDEEFF", "clearkeys");
  row("keya FFFFFFFFFFFF", "keyb FFFFFFFFFFFF");
  row("use a / use b",     "");

  Serial.println();
  Serial.println("Read / Write:");
  row("rhex <block>",      "rasc <block>");
  row("whex <block> <hex>", "wfull <block> <32hex>");
  row("wkeya <sec> <key>", "wkeyb <sec> <key>");

  Serial.println();
  Serial.println("Access:");
  row("setaccess <sec> open",      "setaccess <sec> readonly");
  row("setaccess <sec> keyb_write","CONFIRM / cancel");

  Serial.println();
  Serial.println("Sectors:");
  row("autosec <sec>",     "automap");
  row("dumpsec <sec>",     "dump");
  row("trailer <sec>",     "access <sec>");

  Serial.println();
  Serial.println("Value / Other:");
  row("value <block>",     "halt");
  row("help",              "");

  Serial.println();
}

void handleLine(String line) {
  line.trim();
  if (!line.length()) return;

  int sp1 = line.indexOf(' ');
  String cmd  = (sp1 < 0) ? line : line.substring(0, sp1);
  String rest = (sp1 < 0) ? ""   : line.substring(sp1 + 1);

  cmd.toLowerCase();
  rest.trim();

  // ---------------- HELP ----------------
  if (cmd == "help") {
    printHelp();
  }

  // ---------------- CARD ----------------
  else if (cmd == "info") {
    cmdInfo();
  }

  // ---------------- KEYS ----------------
  else if (cmd == "keys") {
    cmdKeys();
  }
  else if (cmd == "addkey") {
    addKnownKeyHex(rest);
    Serial.println("Re-running automap after adding key...");
    cmdAutoMap();
  }
  else if (cmd == "clearkeys") {
    keyCount = 0;
    Serial.println("Known key list cleared");
  }
  else if (cmd == "keya") {
    setManualKey(false, rest);
  }
else if (cmd == "setaccess") {
  int sp2 = rest.indexOf(' ');
  if (sp2 < 0) {
    Serial.println("Usage: setaccess <sector> open|readonly|keyb_write");
    return;
  }

  byte sector = rest.substring(0, sp2).toInt();
  String mode = rest.substring(sp2 + 1);
  cmdSetAccessPreview(sector, mode);
}
else if (cmd == "confirm") {
  cmdConfirmAccess();
}
else if (cmd == "cancel") {
  cmdCancelAccess();
}  
  else if (cmd == "keyb") {
    setManualKey(true, rest);
  }
  else if (cmd == "use") {
    rest.toLowerCase();

    if (rest == "a") {
      useKeyB = false;
      Serial.println("Using Key A");
    } else if (rest == "b") {
      useKeyB = true;
      Serial.println("Using Key B");
    } else {
      Serial.println("Usage: use a / use b");
    }
  }

  // ---------------- SAFE KEY WRITE ----------------
  else if (cmd == "wkeya") {
    int sp2 = rest.indexOf(' ');
    if (sp2 < 0) {
      Serial.println("Usage: wkeya <sector> <12 hex chars>");
      return;
    }

    byte sector = rest.substring(0, sp2).toInt();
    String hex  = rest.substring(sp2 + 1);

    cmdWriteKey(sector, false, hex);
  }
  else if (cmd == "wkeyb") {
    int sp2 = rest.indexOf(' ');
    if (sp2 < 0) {
      Serial.println("Usage: wkeyb <sector> <12 hex chars>");
      return;
    }

    byte sector = rest.substring(0, sp2).toInt();
    String hex  = rest.substring(sp2 + 1);

    cmdWriteKey(sector, true, hex);
  }

  // ---------------- READ / WRITE ----------------
  else if (cmd == "rhex") {
    cmdReadHex(rest.toInt());
  }
  else if (cmd == "rasc") {
    cmdReadAscii(rest.toInt());
  }
  else if (cmd == "whex") {
    int sp2 = rest.indexOf(' ');
    if (sp2 < 0) {
      Serial.println("Usage: whex <block> <hex>");
      return;
    }

    byte block = rest.substring(0, sp2).toInt();
    String hex = rest.substring(sp2 + 1);

    cmdWritePartial(block, hex);
  }
  else if (cmd == "wfull") {
    int sp2 = rest.indexOf(' ');
    if (sp2 < 0) {
      Serial.println("Usage: wfull <block> <32 hex chars>");
      return;
    }

    byte block = rest.substring(0, sp2).toInt();
    String hex = rest.substring(sp2 + 1);

    cmdWriteFull(block, hex);
  }

  // ---------------- SECTORS ----------------
  else if (cmd == "autosec") {
    trySectorKey(rest.toInt());
  }
  else if (cmd == "automap") {
    cmdAutoMap();
  }
  else if (cmd == "dumpsec") {
    cmdDumpSector(rest.toInt());
  }
  else if (cmd == "dump") {
    cmdInstantDump();   // <-- FAST dump
  }
  else if (cmd == "trailer") {
    cmdTrailer(rest.toInt());
  }
  else if (cmd == "access") {
    cmdAccess(rest.toInt());
  }

  // ---------------- VALUE ----------------
  else if (cmd == "value") {
    cmdValue(rest.toInt());
  }

  // ---------------- OTHER ----------------
  else if (cmd == "halt") {
    nfc.ReleaseCard();
    nfc.SwitchOffRfField();
    Serial.println("Released card / RF field off");
  }

  else {
    Serial.println("Unknown command. Type help.");
  }
}

// ---------- setup / loop ----------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("PN532 MIFARE Classic 1K Serial Shell V001");
  Serial.println("ESP32 SPI: SCK=18 MISO=19 MOSI=23 CS=5");

  nfc.InitHardwareSPI(PN532_CS_PIN, PN532_RESET_PIN);
  nfc.begin();

  byte IC, vh, vl, flags;
  if (nfc.GetFirmwareVersion(&IC, &vh, &vl, &flags)) {
    Serial.print("Reader: PN5");
    printHexByte(IC);
    Serial.print(" firmware ");
    Serial.print(vh);
    Serial.print(".");
    Serial.println(vl);
  } else {
    Serial.println("GetFirmwareVersion failed");
  }

  if (!nfc.SetPassiveActivationRetries()) Serial.println("SetPassiveActivationRetries failed");
  if (!nfc.SamConfig()) Serial.println("SAMConfig failed");

loadDefaultKeys();
clearSectorMap();

Serial.print("FW: ");
const char* p = strrchr(__FILE__, '/');
const char* q = strrchr(__FILE__, '\\');
Serial.print((p > q ? p : q) ? (p > q ? p : q) + 1 : __FILE__);

Serial.print(" | ");
Serial.print(__DATE__);
Serial.print(" ");
Serial.println(__TIME__);

Serial.println("Ready. Type help.");
Serial.println("Present card within 5 seconds for boot automap...");

unsigned long t0 = millis();
while (millis() - t0 < 5000) {
  if (selectCard()) {
    nfc.ReleaseCard();
    cmdAutoMap();
    break;
  }
  delay(200);
}
}

void loop() {
  static String line;

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (line.length()) {
        handleLine(line);
        line = "";
      }
    } else {
      line += c;
    }
  }
}
