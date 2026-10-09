/*
  ESP32 + Elmü PN532 SPI + MIFARE Ultralight-C console

  Commands:
    listall
    dumpcard
    auth [hexkey]
    newkey <hexkey>
    wchar <page> <text>
    whex <page> <hex>
    protect <page>
    setpbit <0|1>

  PN532 SPI:
    SCK  = 18
    MISO = 19
    MOSI = 23
    SS   = 5

  Uses Elmü PN532 library:
    #include "PN532.h"

  Important:
    - PN532 dataExchange() uses InDataExchange.
    - Do NOT append ISO14443 CRC bytes yourself.
    - Ultralight-C AUTH commands are sent as:
        1A 00
        AF <16 encrypted bytes>
*/

#include <Arduino.h>
#include <SPI.h>
#include "PN532.h"
#include "des.h"

#define PN532_SS     5
#define PN532_RESET  0xFF

PN532 nfc;

bool result;
bool isLocked[255];
byte startProtect = 255;
bool entry = false;
bool protectInfoKnown = false;
bool isUlC = true;
int StartPage = 4;
bool authactive = false;
bool is_card_present = false;

uint8_t currentUid[8];
byte currentUidLen = 0;
eCardType currentCardType = CARD_Unknown;

unsigned long cardSettleUntilMs = 0;
byte cardMissingCount = 0;

byte key[16] = {
  0x49, 0x45, 0x4D, 0x4B, 0x41, 0x45, 0x52, 0x42,
  0x21, 0x4E, 0x41, 0x43, 0x55, 0x4F, 0x59, 0x46
};

String serialLine = "";
String addstring = "";
int str_len = 0;

// ---------- Forward declarations ----------
void processSerial();
void handleCommand(String line);
String firstToken(String s);
String restTokens(String s);

bool selectCardForCommand();
void clearCardSession();
void noteCardActivity(unsigned long settleMs = 300);

bool pn532ReadPage4(byte page, byte out4[4]);
bool pn532WritePage4(byte page, const byte data4[4]);

bool ulcAuthenticate(const byte key16[16]);
bool ulcWriteKey(const byte key16[16]);

bool getPageInfo();
bool LearnProtectionInfoAtBoot();
byte DumpDataUltralightC(void);

void Cmd_ListAll();
void Cmd_dumpcard();
void Cmd_auth(const String &params);
void Cmd_newkey(const String &params);
void Cmd_wchar(const String &params);
void Cmd_whex(const String &params);
void Cmd_protect(const String &params);
void Cmd_setpbit(const String &params);
void Cmd_Unknown();

void protect(bool mode);
void WriteToCard(boolean mode);
byte char2byte(char *s);
boolean setAuthKey(void);
void PrintHex(uint8_t *data, uint8_t length);

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println(F("Elmü PN532 Ultralight-C console starting"));

  nfc.InitHardwareSPI(PN532_SS, PN532_RESET);
  nfc.begin();

  byte ic, verHi, verLo, flags;
  if (!nfc.GetFirmwareVersion(&ic, &verHi, &verLo, &flags)) {
    Serial.println(F("PN532 not found. Check wiring/SPI switch."));
    while (1) delay(100);
  }

  Serial.print(F("PN532 firmware IC=0x"));
  Serial.print(ic, HEX);
  Serial.print(F(" version "));
  Serial.print(verHi);
  Serial.print(F("."));
  Serial.println(verLo);

  if (!nfc.SamConfig()) {
    Serial.println(F("SAMConfig failed"));
    while (1) delay(100);
  }

  nfc.SetPassiveActivationRetries();

  Serial.println(F("Ultralight-C console ready"));
  Serial.println(F("Commands: listall, dumpcard, auth [hexkey], newkey <hexkey>, wchar <page> <text>, whex <page> <hex>, protect <page>, setpbit <0|1>"));
}

// ---------- Main loop ----------
void loop() {
  processSerial();

  if (!is_card_present) {
    byte uidLen = 0;
    eCardType cardType = CARD_Unknown;

    bool ok = nfc.ReadPassiveTargetID(currentUid, &uidLen, &cardType);

    if (ok && uidLen > 0) {
      currentUidLen = uidLen;
      currentCardType = cardType;

      is_card_present = true;
      authactive = false;
      protectInfoKnown = false;
      cardMissingCount = 0;

      Serial.print(F("NewCard "));
      PrintHex(currentUid, currentUidLen);
      Serial.println();

      if (LearnProtectionInfoAtBoot()) {
        Serial.print(F("Protect info learned: AUTH0="));
        Serial.print(startProtect);
        Serial.print(F(" PROT="));
        Serial.println(entry);
      } else {
        Serial.println(F("Could not learn protection info"));
      }

      noteCardActivity(500);
    }

    delay(20);
    return;
  }

  if (authactive) {
    delay(50);
    return;
  }

  if (millis() < cardSettleUntilMs) {
    delay(20);
    return;
  }

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType = CARD_Unknown;

  bool ok = nfc.ReadPassiveTargetID(uid, &uidLen, &cardType);

  if (ok && uidLen > 0) {
    cardMissingCount = 0;
  } else {
    cardMissingCount++;

    if (cardMissingCount >= 3) {
      Serial.println(F("Card removed"));
      clearCardSession();
      delay(100);
      return;
    }
  }

  delay(50);
}

void noteCardActivity(unsigned long settleMs) {
  cardSettleUntilMs = millis() + settleMs;
  cardMissingCount = 0;
}

bool selectCardForCommand() {
  byte uid[8];
  byte uidLen = 0;
  eCardType cardType = CARD_Unknown;

  bool ok = nfc.ReadPassiveTargetID(uid, &uidLen, &cardType);

  if (!ok || uidLen == 0) {
    Serial.println(F("Command failed: could not select card"));
    clearCardSession();
    return false;
  }

  memcpy(currentUid, uid, uidLen);
  currentUidLen = uidLen;
  currentCardType = cardType;
  is_card_present = true;

  return true;
}

void clearCardSession() {
  nfc.ReleaseCard();

  authactive = false;
  is_card_present = false;
  protectInfoKnown = false;
  cardMissingCount = 0;
}

// ---------- Raw Ultralight page read/write via Elmü dataExchange ----------
bool pn532ReadPage4(byte page, byte out4[4]) {
  byte cmd[2] = {0x30, page};
  byte rx[24];
  byte rxLen = 0;

  if (!nfc.dataExchange(cmd, sizeof(cmd), rx, &rxLen, sizeof(rx))) {
    return false;
  }

  if (rxLen < 4) {
    return false;
  }

  memcpy(out4, rx, 4);
  return true;
}

bool pn532Read4Pages(byte page, byte out16[16]) {
  byte cmd[2] = {0x30, page};
  byte rx[24];
  byte rxLen = 0;

  if (!nfc.dataExchange(cmd, sizeof(cmd), rx, &rxLen, sizeof(rx))) {
    return false;
  }

  if (rxLen < 16) {
    return false;
  }

  memcpy(out16, rx, 16);
  return true;
}

bool pn532WritePage4(byte page, const byte data4[4]) {
  byte cmd[6];
  cmd[0] = 0xA2;
  cmd[1] = page;
  memcpy(cmd + 2, data4, 4);

  byte rx[8];
  byte rxLen = 0;

  bool ok = nfc.dataExchange(cmd, sizeof(cmd), rx, &rxLen, sizeof(rx));
  delay(8);
  return ok;
}

// ---------- Ultralight-C 3DES authentication ----------
bool des3CbcCrypt(bool encrypt, const byte key16[16], byte iv[8], const byte *in, byte *out, size_t len) {
  mbedtls_des3_context des;
  mbedtls_des3_init(&des);

  int rc = 0;

  if (encrypt) {
    rc = mbedtls_des3_set2key_enc(&des, key16);
    if (rc == 0) {
      rc = mbedtls_des3_crypt_cbc(&des, MBEDTLS_DES_ENCRYPT, len, iv, in, out);
    }
  } else {
    rc = mbedtls_des3_set2key_dec(&des, key16);
    if (rc == 0) {
      rc = mbedtls_des3_crypt_cbc(&des, MBEDTLS_DES_DECRYPT, len, iv, in, out);
    }
  }

  mbedtls_des3_free(&des);
  return rc == 0;
}

void rotateLeft8(const byte in[8], byte out[8]) {
  memcpy(out, in + 1, 7);
  out[7] = in[0];
}

bool ulcAuthenticate(const byte key16[16]) {
  byte cmd1[2] = {0x1A, 0x00};
  byte rx1[16];
  byte rxLen1 = 0;

  if (!nfc.dataExchange(cmd1, sizeof(cmd1), rx1, &rxLen1, sizeof(rx1))) {
    return false;
  }

  if (rxLen1 != 9 || rx1[0] != 0xAF) {
    return false;
  }

  byte ekRndB[8];
  memcpy(ekRndB, rx1 + 1, 8);

  byte iv[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  byte dekRndB[8];

  if (!des3CbcCrypt(false, key16, iv, ekRndB, dekRndB, 8)) {
    return false;
  }

  byte rndA[8];
  for (int i = 0; i < 8; i++) {
    rndA[i] = (byte)(esp_random() & 0xFF);
  }

  byte rndBrot[8];
  rotateLeft8(dekRndB, rndBrot);

  byte rndARndBrot[16];
  memcpy(rndARndBrot, rndA, 8);
  memcpy(rndARndBrot + 8, rndBrot, 8);

  // mbedTLS CBC decrypt above changed IV to ekRndB, same as the RC522 library behaviour.
  byte ekRndARndBrot[16];
  if (!des3CbcCrypt(true, key16, iv, rndARndBrot, ekRndARndBrot, 16)) {
    return false;
  }

  byte cmd2[17];
  cmd2[0] = 0xAF;
  memcpy(cmd2 + 1, ekRndARndBrot, 16);

  byte rx2[16];
  byte rxLen2 = 0;

  if (!nfc.dataExchange(cmd2, sizeof(cmd2), rx2, &rxLen2, sizeof(rx2))) {
    return false;
  }

  if (rxLen2 != 9) {
    return false;
  }

  // Some cards/libraries return 00 + enc(RndA'), some return AF + data in broken cases.
  if (rx2[0] != 0x00) {
    return false;
  }

  byte rndArot[8];
  rotateLeft8(rndA, rndArot);

  byte verifyIv[8];
  memcpy(verifyIv, ekRndARndBrot + 8, 8);

  byte expectedEkRndArot[8];
  if (!des3CbcCrypt(true, key16, verifyIv, rndArot, expectedEkRndArot, 8)) {
    return false;
  }

  return memcmp(rx2 + 1, expectedEkRndArot, 8) == 0;
}

// Correct Ultralight-C storage order:
// page 44 = key[7..4]
// page 45 = key[3..0]
// page 46 = key[15..12]
// page 47 = key[11..8]
bool ulcWriteKey(const byte key16[16]) {
  byte p44[4] = {key16[7],  key16[6],  key16[5],  key16[4]};
  byte p45[4] = {key16[3],  key16[2],  key16[1],  key16[0]};
  byte p46[4] = {key16[15], key16[14], key16[13], key16[12]};
  byte p47[4] = {key16[11], key16[10], key16[9],  key16[8]};

  if (!pn532WritePage4(44, p44)) return false;
  if (!pn532WritePage4(45, p45)) return false;
  if (!pn532WritePage4(46, p46)) return false;
  if (!pn532WritePage4(47, p47)) return false;

  return true;
}

// ---------- Serial command parser ----------
void processSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      serialLine.trim();

      if (serialLine.length() > 0) {
        handleCommand(serialLine);
      }

      serialLine = "";
    } else {
      serialLine += c;

      if (serialLine.length() > 120) {
        serialLine = "";
      }
    }
  }
}

String firstToken(String s) {
  s.trim();
  int p = s.indexOf(' ');
  if (p < 0) return s;
  return s.substring(0, p);
}

String restTokens(String s) {
  s.trim();
  int p = s.indexOf(' ');
  if (p < 0) return "";
  String r = s.substring(p + 1);
  r.trim();
  return r;
}

void handleCommand(String line) {
  String cmd = firstToken(line);
  String params = restTokens(line);
  cmd.toLowerCase();

  if (cmd == "listall") {
    Cmd_ListAll();
  } else if (cmd == "dumpcard") {
    Cmd_dumpcard();
  } else if (cmd == "auth") {
    Cmd_auth(params);
  } else if (cmd == "newkey") {
    Cmd_newkey(params);
  } else if (cmd == "wchar") {
    Cmd_wchar(params);
  } else if (cmd == "whex") {
    Cmd_whex(params);
  } else if (cmd == "protect") {
    Cmd_protect(params);
  } else if (cmd == "setpbit") {
    Cmd_setpbit(params);
  } else {
    Cmd_Unknown();
  }
}

// ---------- Boot protection info ----------
bool LearnProtectionInfoAtBoot() {
  if (!selectCardForCommand()) return false;

  if (!ulcAuthenticate(key)) {
    authactive = false;
    noteCardActivity(300);
    return false;
  }

  authactive = true;

  byte p42[4];
  byte p43[4];

  if (!pn532ReadPage4(42, p42)) {
    authactive = false;
    noteCardActivity(300);
    return false;
  }

  if (!pn532ReadPage4(43, p43)) {
    authactive = false;
    noteCardActivity(300);
    return false;
  }

  startProtect = p42[0];
  entry = p43[0] & 0x01;
  protectInfoKnown = true;

  authactive = false;
  nfc.ReleaseCard();
  noteCardActivity(500);

  return true;
}

// ---------- Dump ----------
byte DumpDataUltralightC(void) {
  for (int j = 0; j < 255; j++) {
    isLocked[j] = false;
  }

  getPageInfo();

  Serial.println(F("Page lock auth   0  1  2  3   0 1 2 3"));

  for (byte page = 0; page < 43; page++) {
    byte b4[4] = {0};
    bool ok = pn532ReadPage4(page, b4);

    bool protectedPage = protectInfoKnown && (page >= startProtect);

    if (page < 10) Serial.print("  ");
    else           Serial.print(" ");
    Serial.print(page);

    if (isLocked[page]) Serial.print("   x ");
    else                Serial.print("     ");

    if (protectedPage) {
      if (entry) Serial.print("  r/o ");
      else       Serial.print("  r/w ");
    } else {
      Serial.print("      ");
    }

    for (byte index = 0; index < 4; index++) {
      if (!authactive && protectedPage) {
        Serial.print(" XX");
      } else if (!ok) {
        Serial.print(" 00");
      } else {
        if (b4[index] < 0x10) Serial.print(" 0");
        else                  Serial.print(" ");
        Serial.print(b4[index], HEX);
      }
    }

    Serial.print("   ");

    for (byte index = 0; index < 4; index++) {
      Serial.print(" ");

      if (!authactive && protectedPage) {
        Serial.print("X");
      } else if (!ok) {
        Serial.print(".");
      } else {
        if (b4[index] > 0x20 && b4[index] < 0x7F) {
          Serial.print((char)b4[index]);
        } else {
          Serial.print(".");
        }
      }
    }

    Serial.println();
  }

  for (byte page = 44; page <= 47; page++) {
    Serial.print(" ");
    Serial.print(page);
    Serial.print(F(" XXXXXXXXXXX "));

    int start = (page - 44) * 4;

    for (int i = start; i < start + 4; i++) {
      if (key[i] < 0x10) Serial.print("0");
      Serial.print(key[i], HEX);
      Serial.print(" ");
    }

    Serial.print("  ");

    for (int i = start; i < start + 4; i++) {
      if (key[i] > 0x20 && key[i] < 0x7F) Serial.print((char)key[i]);
      else                                Serial.print(".");
      Serial.print(" ");
    }

    Serial.println();
  }

  return 0;
}

bool getPageInfo() {
  int i;
  uint32_t mask = 0;

  byte p2[4];
  if (!pn532ReadPage4(2, p2)) {
    return false;
  }

  mask = p2[3];
  mask = (mask << 8) | p2[2];

  isLocked[0] = 1;
  isLocked[1] = 1;
  isLocked[2] = 1;

  if (mask & 0x0001) isLocked[3] = 1;
  mask >>= 1;

  if (mask & 0x0001) {
    for (i = 0; i < 6; i++) isLocked[4 + i] = 1;
  }
  mask >>= 1;

  if (mask & 0x0001) {
    for (i = 0; i < 6; i++) isLocked[10 + i] = 1;
  }
  mask >>= 1;

  for (i = 0; i < 13; i++) {
    if (mask & 0x0001) isLocked[3 + i] = 1;
    mask >>= 1;
  }

  return true;
}

// ---------- Commands ----------
void Cmd_ListAll() {
  Serial.println(F("dumpcard"));
  Serial.println(F("wchar"));
  Serial.println(F("auth"));
  Serial.println(F("newkey"));
  Serial.println(F("whex"));
  Serial.println(F("protect"));
  Serial.println(F("setpbit"));
}

void Cmd_dumpcard() {
  Serial.println(F("Dumping contents"));

  if (!authactive) {
    if (!selectCardForCommand()) return;
  }

  DumpDataUltralightC();

  authactive = false;
  nfc.ReleaseCard();
  noteCardActivity(400);

  Serial.println(F("Dump complete"));
}

void Cmd_auth(const String &params) {
  addstring = params;
  Serial.println(F("Authenticate card"));

  result = setAuthKey();

  if (!result) {
    Serial.println(F("Authentication failed"));
    return;
  }

  if (!selectCardForCommand()) {
    authactive = false;
    noteCardActivity(400);
    return;
  }

  result = ulcAuthenticate(key);

  if (result) {
    authactive = true;

    byte p42[4];
    byte p43[4];

    if (pn532ReadPage4(42, p42)) {
      startProtect = p42[0];
      protectInfoKnown = true;
    }

    if (pn532ReadPage4(43, p43)) {
      entry = p43[0] & 0x01;
      protectInfoKnown = true;
    }

    Serial.println(F("Authentication successfull"));
    noteCardActivity(400);
  } else {
    authactive = false;
    Serial.println(F("Authentication failed"));
    nfc.ReleaseCard();
    noteCardActivity(400);
  }
}

void Cmd_newkey(const String &params) {
  addstring = params;
  Serial.println(F("Write New Key to card"));

  if (!authactive) {
    Serial.println(F("Use auth first before newkey."));
    return;
  }

  if (!setAuthKey()) {
    Serial.println(F("Invalid key."));
    return;
  }

  result = ulcWriteKey(key);

  if (result) Serial.println(F("Write Key successfull"));
  else        Serial.println(F("Write Key failed"));
}

void Cmd_wchar(const String &params) {
  int p = params.indexOf(' ');

  if (p < 0) {
    Serial.println(F("Usage: wchar <page> <text>"));
    return;
  }

  StartPage = params.substring(0, p).toInt();
  addstring = params.substring(p + 1);

  Serial.print(F("Wchar "));
  Serial.print(StartPage);
  Serial.print(" ");
  Serial.println(addstring);

  if (!selectCardForCommand()) return;

  if (authactive) {
    result = ulcAuthenticate(key);

    if (!result) {
      Serial.println(F("Write auth failed"));
      authactive = false;
      return;
    }
  }

  WriteToCard(0);
  noteCardActivity(300);
}

void Cmd_whex(const String &params) {
  int p = params.indexOf(' ');

  if (p < 0) {
    Serial.println(F("Usage: whex <page> <hex>"));
    return;
  }

  StartPage = params.substring(0, p).toInt();
  addstring = params.substring(p + 1);

  Serial.print(F("Whex StartPage "));
  Serial.print(StartPage);
  Serial.print(" ");
  Serial.println(addstring);

  if (!selectCardForCommand()) return;

  if (authactive) {
    result = ulcAuthenticate(key);

    if (!result) {
      Serial.println(F("Write auth failed"));
      authactive = false;
      return;
    }
  }

  WriteToCard(1);
  noteCardActivity(300);
}

void Cmd_protect(const String &params) {
  StartPage = params.toInt();

  Serial.print(F("Protect "));
  Serial.println(StartPage);

  if (!selectCardForCommand()) return;

  if (authactive) {
    result = ulcAuthenticate(key);

    if (!result) {
      Serial.println(F("Protect auth failed"));
      authactive = false;
      return;
    }
  }

  protect(1);
}

void Cmd_setpbit(const String &params) {
  StartPage = params.toInt();

  if (StartPage == 1) {
    Serial.println(F("Setpbit 1 means Protected pages are read only"));
  } else if (StartPage == 0) {
    Serial.println(F("Setpbit 0 means Protected pages can not be read or written"));
  } else {
    Serial.println(F("Setpbit Error: pbit must be 0 or 1"));
    return;
  }

  if (!selectCardForCommand()) return;

  if (authactive) {
    result = ulcAuthenticate(key);

    if (!result) {
      Serial.println(F("Setpbit auth failed"));
      authactive = false;
      return;
    }
  }

  protect(0);
}

void Cmd_Unknown() {
  Serial.println(F("I don't understand"));
}

// ---------- Protect / write helpers ----------
void protect(bool mode) {
  getPageInfo();

  byte cfgPage;
  if (mode) cfgPage = 42;
  else      cfgPage = 43;

  if (isLocked[cfgPage]) {
    Serial.println(F("PICC is write protected"));
    return;
  }

  byte value = StartPage;

  if (mode) {
    if (value > 48) {
      value = 48;
      Serial.println(F("Max page = 48"));
    }

    if (value < 4) {
      value = 4;
      Serial.println(F("Min page = 4"));
    }
  } else {
    if (value > 1) value = 1;
  }

  byte pageData[4] = {0, 0, 0, 0};
  pageData[0] = value;

  if (!pn532WritePage4(cfgPage, pageData)) {
    if (mode) Serial.println(F("ERROR: password protection not set"));
    else      Serial.println(F("ERROR: protection bit not set"));
    return;
  }

  byte verify[4];

  if (mode) {
    if (pn532ReadPage4(42, verify)) {
      startProtect = verify[0];
      protectInfoKnown = true;

      Serial.print(F("password protection starts now at page: "));
      Serial.println(startProtect);
    } else {
      Serial.println(F("WARNING: wrote AUTH0 but could not verify"));
    }
  } else {
    if (pn532ReadPage4(43, verify)) {
      entry = verify[0] & 0x01;
      protectInfoKnown = true;

      Serial.print(F("protection bit is set to: "));
      if (entry) Serial.println(F("read only without auth"));
      else       Serial.println(F("no read/write without auth"));
    } else {
      Serial.println(F("WARNING: wrote ACCESS but could not verify"));
    }
  }
}

void WriteToCard(boolean mode) {
  getPageInfo();

  byte page = StartPage;
  byte Buffer[4] = {0, 0, 0, 0};

  if (isUlC) Serial.println(F("Ultralight C"));
  else       Serial.println(F("not a Ultralight C PICC!"));

  if (page < 4) {
    Serial.println(F("no user memory here"));
    return;
  }

  if (isUlC && (page > 39)) {
    Serial.println(F("no user memory here"));
    return;
  }

  int str_len_local = addstring.length() + 1;
  char SdataLocal[str_len_local];
  addstring.toCharArray(SdataLocal, str_len_local);

  byte i = 0;
  boolean done = 0;

  for (; page < 40; page++) {
    for (byte j = 0; j < 4; j++) {
      if (SdataLocal[i] == 0x00) {
        while (j < 4) {
          Buffer[j] = 0;
          j++;
        }
        done = 1;
      } else {
        if (mode) {
          Buffer[j] = char2byte(SdataLocal + i);
          i = i + 2;
        } else {
          Buffer[j] = SdataLocal[i];
          i++;
        }
      }
    }

    Serial.print(F("Page "));
    Serial.print(page);
    Serial.print(" ");

    for (int k = 0; k < 4; k++) {
      if (Buffer[k] < 0x10) Serial.print("0");
      Serial.print(Buffer[k], HEX);
      Serial.print(" ");
    }

    Serial.println();

    if (!pn532WritePage4(page, Buffer)) {
      Serial.println(F("Error: bytes not written. Authentication may be required."));
      return;
    }

    if (done) break;
  }

  Serial.println(F("Done"));
}

// ---------- Hex/key helpers ----------
byte char2byte(char *s) {
  byte x = 0;

  for (int i = 0; i < 2; i++) {
    char c = *s;

    if (c >= '0' && c <= '9') {
      x *= 16;
      x += c - '0';
    } else if (c >= 'A' && c <= 'F') {
      x *= 16;
      x += (c - 'A') + 10;
    } else if (c >= 'a' && c <= 'f') {
      x *= 16;
      x += (c - 'a') + 10;
    }

    s++;
  }

  return x;
}

boolean setAuthKey(void) {
  str_len = addstring.length() + 1;

  if (str_len < 8) {
    Serial.println(F("Use STD key"));

    byte stdkey[16] = {
      0x49, 0x45, 0x4D, 0x4B, 0x41, 0x45, 0x52, 0x42,
      0x21, 0x4E, 0x41, 0x43, 0x55, 0x4F, 0x59, 0x46
    };

    memcpy(key, stdkey, 16);
    str_len = 32;
  } else {
    char SdataKey[str_len];
    addstring.toCharArray(SdataKey, str_len);

    for (int i = 0; i < 32; i++) {
      if ((i % 2 == 0)) {
        key[i / 2] = char2byte(&SdataKey[i]);
      }
    }
  }

  if (str_len < 32) {
    Serial.println(F("Key too short! needs 32 hex digits"));
    return 0;
  }

  Serial.print(F("Authenticate key = "));

  for (int i = 0; i < 16; i++) {
    if (key[i] < 0x10) Serial.print("0");
    Serial.print(key[i], HEX);
  }

  Serial.println();
  return 1;
}

void PrintHex(uint8_t *data, uint8_t length) {
  char tmp[16];

  for (int i = 0; i < length; i++) {
    sprintf(tmp, "0x%.2X", data[i]);
    Serial.print(tmp);
    Serial.print(" ");
  }
}
