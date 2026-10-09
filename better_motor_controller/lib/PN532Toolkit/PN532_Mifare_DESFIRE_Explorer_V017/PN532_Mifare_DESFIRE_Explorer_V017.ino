#include <WiFi.h>
#include <WebServer.h>

#include "Utils.h"
#include "PN532.h"
#include "Desfire.h"
#include "Secrets.h"
#include "Buffer.h"
#include "UserManager.h"
#include <EEPROM.h>

#define PN532_CS_PIN     5
#define PN532_RESET_PIN  255
#define KEYSTORE_MAGIC 0xB54B
#define KEYSTORE_MAX   8
#define KEYSTORE_EEPROM_ADDR 512

const char* WIFI_SSID = "Ziggo8773552";
const char* WIFI_PASS = "xYf4bntkdjdu";

struct StoredAppKey {
  uint16_t magic;
  bool used;
  uint32_t aid;
  byte keyNo;
  byte version;
  byte key[24];
};

StoredAppKey keyStore[KEYSTORE_MAX];
String cardTypeName(int t);
bool authenticateSelectedApp(uint32_t aid, byte uid[8], byte uidLen, String &h, byte keyNo = 0);

WebServer server(80);
Desfire gi_PN532;

String hexByte(byte b) {
  char buf[4];
  sprintf(buf, "%02X", b);
  return String(buf);
}

String hexBuf(const byte *buf, byte len) {
  String s;
  for (byte i = 0; i < len; i++) {
    if (i) s += " ";
    s += hexByte(buf[i]);
  }
  return s;
}

String pageHeader(const String &title) {
  String h;
  h += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<title>" + title + "</title>";
  h += "<style>";
  h += "body{font-family:Arial;background:#111;color:#eee;margin:20px}";
  h += "a{color:#7cf;text-decoration:none} .card{background:#1b1b1b;border:1px solid #333;border-radius:12px;padding:14px;margin:12px 0}";
  h += "pre{white-space:pre-wrap;background:#080808;padding:12px;border-radius:8px}";
  h += ".ok{color:#6f6}.bad{color:#f66}.btn{display:inline-block;background:#333;padding:8px 12px;border-radius:8px;margin:4px}";
  h += "</style></head><body>";
  h += "<h2>" + title + "</h2>";
  h += "<p><a class='btn' href='/'>Home</a><a class='btn' href='/reader'>Reader</a><a class='btn' href='/card'>Card</a><a class='btn' href='/picc'>PICC</a><a class='btn' href='/apps'>Apps</a><a class='btn' href='/dump'>Dump</a></p>";
  h += "<p>";
  h += "<a class='btn' href='/createapp'>Create app</a>";
  h += "<a class='btn' href='/createfile'>Create file</a>";
  h += "<a class='btn' href='/deleteapp'>Delete app</a>";
  h += "</p>";
  h += "<p>";
  h += "<a class='btn' href='/listkeys'>Keys</a>";
  h += "<a class='btn' href='/changeappkey'>Change app key</a>";
  h += "<a class='btn' href='/delkey'>Delete stored key</a>";
  h += "</p>";
  return h;
}

String pageFooter() {
  return "</body></html>";
}

void releaseCard() {
  gi_PN532.SwitchOffRfField();
}

bool readCard(byte uid[8], byte *uidLen, eCardType *cardType, String &out) {
  *uidLen = 0;

  if (!gi_PN532.ReadPassiveTargetID(uid, uidLen, cardType)) {
    out += "<p class='bad'>No card / read failed</p>";
    return false;
  }

  if (*uidLen == 0) {
    out += "<p class='bad'>No card present</p>";
    return false;
  }

  out += "<p class='ok'>Card detected</p>";
  out += "<p>UID: <b>" + hexBuf(uid, *uidLen) + "</b></p>";
  out += "<p>UID length: " + String(*uidLen) + "</p>";
  out += "<p>Card type: <b>";
  out += cardTypeName((int)*cardType);
  out += "</b> (" + String((int)*cardType) + ")</p>";
  

  return true;
}

void handleHome() {
  String h = pageHeader("ESP32 DESFire Web Workbench V001");
  h += "<div class='card'>";
  h += "<p>Read-only DES/3K3DES DESFire browser.</p>";
  h += "<p>No formatting, no key changes, no writes.</p>";
  h += "<p>WiFi IP: <b>" + WiFi.localIP().toString() + "</b></p>";
  h += "</div>";
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleReader() {
  String h = pageHeader("Reader");

  byte IC, vh, vl, flags;
  if (gi_PN532.GetFirmwareVersion(&IC, &vh, &vl, &flags)) {
    h += "<div class='card'>";
    h += "<p>Chip: PN5" + hexByte(IC) + "</p>";
    h += "<p>Firmware: " + String(vh) + "." + String(vl) + "</p>";
    h += "<p>ISO14443A: " + String((flags & 1) ? "yes" : "no") + "</p>";
    h += "<p>ISO14443B: " + String((flags & 2) ? "yes" : "no") + "</p>";
    h += "<p>ISO18092: " + String((flags & 4) ? "yes" : "no") + "</p>";
    h += "</div>";
  } else {
    h += "<p class='bad'>GetFirmwareVersion failed</p>";
  }

  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleCard() {
  String h = pageHeader("Card");

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(0x000000)) {
    h += "<p class='bad'>Select PICC/root failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DESFireCardVersion ver;
  if (gi_PN532.GetCardVersion(&ver)) {
    h += "<div class='card'><h3>DESFire version</h3>";
    h += "<p>HW vendor/type/subtype: " + String(ver.hardwareVendorId) + "/" + String(ver.hardwareType) + "/" + String(ver.hardwareSubType) + "</p>";
    h += "<p>HW version: " + String(ver.hardwareMajVersion) + "." + String(ver.hardwareMinVersion) + "</p>";
    h += "<p>SW vendor/type/subtype: " + String(ver.softwareVendorId) + "/" + String(ver.softwareType) + "/" + String(ver.softwareSubType) + "</p>";
    h += "<p>SW version: " + String(ver.softwareMajVersion) + "." + String(ver.softwareMinVersion) + "</p>";
    h += "<p>Storage size byte: 0x" + hexByte(ver.hardwareStorageSize) + "</p>";
    h += "<p>Production week/year: " + hexByte(ver.cwProd) + "/" + hexByte(ver.yearProd) + "</p>";
    h += "</div>";
  } else {
    h += "<p class='bad'>GetCardVersion failed</p>";
  }

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handlePicc() {
  String h = pageHeader("PICC / root");

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  gi_PN532.SelectApplication(0x000000);

  uint32_t freeMem = 0;
  if (gi_PN532.GetFreeMemory(&freeMem)) {
    h += "<p>Free memory: <b>" + String(freeMem) + "</b> bytes</p>";
  } else {
    h += "<p class='bad'>GetFreeMemory failed</p>";
  }

  DESFireKeySettings ks;
  byte keyCount = 0;
  DESFireKeyType keyType;

  if (gi_PN532.GetKeySettings(&ks, &keyCount, &keyType)) {
    h += "<div class='card'>";
    h += "<p>PICC key settings: 0x" + hexByte((byte)ks) + "</p>";
    h += "<p>PICC key count: " + String(keyCount) + "</p>";
    h += "<p>PICC key type: <b>";
    h += keyTypeName((int)keyType);
    h += "</b> (" + String((int)keyType) + ")</p>";
    h += "</div>";
  } else {
    h += "<p class='bad'>GetKeySettings failed</p>";
  }

  byte keyVer = 0xFF;
  if (gi_PN532.GetKeyVersion(0, &keyVer)) {
    h += "<p>PICC key 0 version: " + String(keyVer) + "</p>";
  } else {
    h += "<p class='bad'>GetKeyVersion(0) failed</p>";
  }

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

String aidToHex(uint32_t aid) {
  return hexByte((aid >> 16) & 0xFF) + hexByte((aid >> 8) & 0xFF) + hexByte(aid & 0xFF);
}

uint32_t parseAid(String s) {
  s.replace("0x", "");
  s.replace(" ", "");
  s.replace(":", "");
  return strtoul(s.c_str(), NULL, 16) & 0xFFFFFF;
}

void loadKeyStore() {
  for (int i = 0; i < KEYSTORE_MAX; i++) {
    EEPROM.get(KEYSTORE_EEPROM_ADDR + i * sizeof(StoredAppKey), keyStore[i]);
    if (keyStore[i].magic != KEYSTORE_MAGIC) {
      memset(&keyStore[i], 0, sizeof(StoredAppKey));
      keyStore[i].magic = KEYSTORE_MAGIC;
    }
  }
}

void handleWriteFileAscii() {
  String h = pageHeader("Write ASCII file");

  if (!server.hasArg("aid") || !server.hasArg("id") || !server.hasArg("ascii")) {
    h += "<p class='bad'>Missing aid, id, or ascii</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);
  byte fileId = server.arg("id").toInt();
  byte authKey = server.hasArg("authkey") ? server.arg("authkey").toInt() : 0;
  String text = server.arg("ascii");

  byte data[256];
  int dataLen = text.length();
  if (dataLen > (int)sizeof(data)) dataLen = sizeof(data);

  for (int i = 0; i < dataLen; i++) {
    data[i] = (byte)text[i];
  }

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(aid)) {
    h += "<p class='bad'>SelectApplication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!authenticateSelectedApp(aid, uid, uidLen, h, authKey)) {
    h += "<p class='bad'>Authentication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DESFireFileSettings fs;
  if (!gi_PN532.GetFileSettings(fileId, &fs)) {
    h += "<p class='bad'>GetFileSettings failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if ((uint32_t)dataLen > fs.u32_FileSize) {
    h += "<p class='bad'>Text is longer than file size</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.WriteFileData(fileId, 0, dataLen, data)) {
    h += "<p class='bad'>WriteFileData failed</p>";
  } else {
    h += "<p class='ok'>ASCII write OK</p>";
    h += "<p>Wrote " + String(dataLen) + " bytes</p>";
  }

  h += "<p><a class='btn' href='/file?aid=" + aidHex + "&id=" + String(fileId) + "&authkey=" + String(authKey) + "'>Back to file</a></p>";

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void saveKeyStoreSlot(int i) {
  EEPROM.put(KEYSTORE_EEPROM_ADDR + i * sizeof(StoredAppKey), keyStore[i]);
  EEPROM.commit();
}

int findStoredKey(uint32_t aid, byte keyNo) {
  for (int i = 0; i < KEYSTORE_MAX; i++) {
    if (keyStore[i].used && keyStore[i].aid == aid && keyStore[i].keyNo == keyNo) return i;
  }
  return -1;
}

int findFreeKeySlot() {
  for (int i = 0; i < KEYSTORE_MAX; i++) {
    if (!keyStore[i].used) return i;
  }
  return -1;
}

String asciiFromBuf(const byte *buf, int len) {
  String s;
  for (int i = 0; i < len; i++) {
    char c = buf[i];
    s += (c >= 32 && c <= 126) ? c : '.';
  }
  return s;
}

bool storeAppKey(uint32_t aid, byte keyNo, const byte *key24, byte version) {
  int i = findStoredKey(aid, keyNo);
  if (i < 0) i = findFreeKeySlot();
  if (i < 0) return false;

  keyStore[i].magic = KEYSTORE_MAGIC;
  keyStore[i].used = true;
  keyStore[i].aid = aid;
  keyStore[i].keyNo = keyNo;
  keyStore[i].version = version;
  memcpy(keyStore[i].key, key24, 24);

  saveKeyStoreSlot(i);
  return true;
}

bool authenticateSelectedApp(uint32_t aid, byte uid[8], byte uidLen, String &h, byte keyNo) {  
  if (keyNo > 13) {
    h += "<p class='bad'>Invalid auth key number. Use 0..13.</p>";
    return false;
  }

  int slot = findStoredKey(aid, keyNo);

  if (slot >= 0) {
    DES storedKey;

    if (storedKey.SetKeyData(keyStore[slot].key, 24, keyStore[slot].version)) {
      if (gi_PN532.Authenticate(keyNo, &storedKey)) {
        h += "<p class='ok'>Authenticated with stored ESP32 key for AID 0x";
        h += aidToHex(aid);
        h += " as key ";
        h += String(keyNo);
        h += "</p>";
        return true;
      } else {
        h += "<p class='bad'>Stored ESP32 key failed for AID 0x";
        h += aidToHex(aid);
        h += " as key ";
        h += String(keyNo);
        h += "</p>";
      }
    } else {
      h += "<p class='bad'>Stored key setup failed</p>";
    }
  }

  if (gi_PN532.Authenticate(keyNo, &gi_PN532.DES3_DEFAULT_KEY)) {
    h += "<p class='ok'>Authenticated with default DES/3K3DES key as key ";
    h += String(keyNo);
    h += "</p>";
    return true;
  }

  h += "<p class='bad'>Default DES/3K3DES key failed as key ";
  h += String(keyNo);
  h += "</p>";

  // Door-opener derived key currently only makes sense as key 0.
  if (keyNo != 0) {
    h += "<p class='bad'>Door-opener derived key is only tried as key 0</p>";
    return false;
  }

  uint64_t uid64 = 0;
  memcpy(&uid64, uid, uidLen > 8 ? 8 : uidLen);

  kUser user;
  DES appMasterKey;
  byte expectedStoreValue[16];

  if (!UserManager::FindUser(uid64, &user)) {
    h += "<p class='bad'>No EEPROM user found for this UID</p>";
    return false;
  }

  h += "<p class='ok'>Matched EEPROM user: <b>";
  h += user.s8_Name;
  h += "</b></p>";

  if (!GenerateDesfireSecrets(&user, &appMasterKey, expectedStoreValue)) {
    h += "<p class='bad'>GenerateDesfireSecrets failed</p>";
    return false;
  }

  if (!gi_PN532.Authenticate(0, &appMasterKey)) {
    h += "<p class='bad'>Derived app authentication failed as key 0</p>";
    return false;
  }

  h += "<p class='ok'>Authenticated with derived door-opener app key as key 0</p>";
  return true;
}

void handleApp() {
  String h = pageHeader("Application");

  if (!server.hasArg("aid")) {
    h += "<p class='bad'>Missing aid parameter</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(aid)) {
    h += "<p class='bad'>SelectApplication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<p>Selected AID: <b>0x" + aidHex + "</b></p>";

  if (!authenticateSelectedApp(aid, uid, uidLen, h)) {
    h += "<p class='bad'>Cannot access protected application without authentication</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DESFireKeySettings ks;
  byte keyCount = 0;
  DESFireKeyType keyType;

  if (gi_PN532.GetKeySettings(&ks, &keyCount, &keyType)) {
    h += "<div class='card'>";
    h += "<h3>AID 0x" + aidHex + "</h3>";
    h += "<p>Key settings: 0x" + hexByte((byte)ks) + "</p>";
    h += "<p>Key count: " + String(keyCount) + "</p>";
    h += "<p>Key type: <b>";
    h += keyTypeName((int)keyType);
    h += "</b> (" + String((int)keyType) + ")</p>";
    h += "</div>";
  } else {
    h += "<p class='bad'>GetKeySettings failed</p>";
  }

  h += "<p><a class='btn' href='/files?aid=" + aidHex + "'>List files</a></p>";

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

bool parseHexToBytes(String s, byte *out, int maxLen, int &outLen) {
  s.replace(" ", "");
  s.replace(":", "");
  s.replace("-", "");
  s.replace("\r", "");
  s.replace("\n", "");

  if (s.length() % 2) return false;

  outLen = s.length() / 2;
  if (outLen > maxLen) return false;

  for (int i = 0; i < outLen; i++) {
    char c1 = s[i * 2];
    char c2 = s[i * 2 + 1];

    int hi = (c1 >= '0' && c1 <= '9') ? c1 - '0' :
             (c1 >= 'A' && c1 <= 'F') ? c1 - 'A' + 10 :
             (c1 >= 'a' && c1 <= 'f') ? c1 - 'a' + 10 : -1;

    int lo = (c2 >= '0' && c2 <= '9') ? c2 - '0' :
             (c2 >= 'A' && c2 <= 'F') ? c2 - 'A' + 10 :
             (c2 >= 'a' && c2 <= 'f') ? c2 - 'a' + 10 : -1;

    if (hi < 0 || lo < 0) return false;

    out[i] = (hi << 4) | lo;
  }

  return true;
}

void handleWriteFile() {
  String h = pageHeader("Write file");

  if (!server.hasArg("aid") || !server.hasArg("id") || !server.hasArg("hex")) {
    h += "<p class='bad'>Missing aid, id, or hex</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);
  byte fileId = server.arg("id").toInt();
  byte authKey = server.hasArg("authkey") ? server.arg("authkey").toInt() : 0;

  byte data[256];
  int dataLen = 0;

  if (!parseHexToBytes(server.arg("hex"), data, sizeof(data), dataLen)) {
    h += "<p class='bad'>Invalid hex data</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(aid)) {
    h += "<p class='bad'>SelectApplication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!authenticateSelectedApp(aid, uid, uidLen, h, authKey)) {
    h += "<p class='bad'>Authentication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DESFireFileSettings fs;
  if (!gi_PN532.GetFileSettings(fileId, &fs)) {
    h += "<p class='bad'>GetFileSettings failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (fs.e_FileType != MDFT_STANDARD_DATA_FILE &&
      fs.e_FileType != MDFT_BACKUP_DATA_FILE) {
    h += "<p class='bad'>Refusing write: not a standard/backup data file</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if ((uint32_t)dataLen > fs.u32_FileSize) {
    h += "<p class='bad'>Refusing write: data longer than file size</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.WriteFileData(fileId, 0, dataLen, data)) {
    h += "<p class='bad'>WriteFileData failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<p class='ok'>Write OK</p>";
  h += "<p>Wrote " + String(dataLen) + " bytes to file " + String(fileId) + " in AID 0x" + aidHex + "</p>";
  h += "<p><a class='btn' href='/file?aid=" + aidHex + "&id=" + String(fileId) + "&authkey=" + String(authKey) + "'>Back to file</a></p>";

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleFiles() {
  String h = pageHeader("Files");

  if (!server.hasArg("aid")) {
    h += "<p class='bad'>Missing aid parameter</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(aid)) {
    h += "<p class='bad'>SelectApplication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<p>Selected AID: <b>0x" + aidHex + "</b></p>";

  if (!authenticateSelectedApp(aid, uid, uidLen, h)) {
    h += "<p class='bad'>Cannot access protected application without authentication</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  byte fileIds[16] = {0};
  byte fileCount = 0;

  if (!gi_PN532.GetFileIDs(fileIds, &fileCount)) {
    h += "<p class='bad'>GetFileIDs failed.</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<div class='card'>";
  h += "<h3>AID 0x" + aidHex + " files: " + String(fileCount) + "</h3>";

  for (byte i = 0; i < fileCount; i++) {
    h += "<p>File ";
    h += String(fileIds[i]);
    h += " <a href='/file?aid=";
    h += aidHex;
    h += "&id=";
    h += String(fileIds[i]);
    h += "'>[inspect]</a></p>";
  }

  h += "</div>";

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

const char* fileTypeName(int t) {
  switch (t) {
    case 0: return "Standard Data File";
    case 1: return "Backup Data File";
    case 2: return "Value File";
    case 3: return "Linear Record File";
    case 4: return "Cyclic Record File";
    default: return "Unknown";
  }
}

const char* commModeName(int m) {
  switch (m) {
    case CM_PLAIN:   return "Plain";
    case CM_MAC:     return "MACed";
    case CM_ENCRYPT: return "Enciphered";
    default:         return "Unknown";
  }
}

String accessRightName(byte k) {
  if (k <= 0x0D) return "Key " + String(k);
  if (k == 0x0E) return "Free";
  if (k == 0x0F) return "Never";
  return "Unknown";
}

String cardTypeName(int t) {
  switch (t) {
    case 0: return "Unknown / none";
    case 1: return "DESFire";
    case 2: return "DESFire random ID";
    case 3: return "MIFARE Classic";
    default: return "Unknown";
  }
}


String keyTypeName(int t) {
  switch (t) {
    case 0:  return "DES / 2K3DES";
    case 64: return "3K3DES";
    case 128:return "AES";
    default: return "Unknown";
  }
}

void handleFile() {
  String h = pageHeader("File");

  if (!server.hasArg("aid") || !server.hasArg("id")) {
    h += "<p class='bad'>Missing aid or id parameter</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  byte fileId = server.arg("id").toInt();
  String aidHex = aidToHex(aid);
  byte authKey = server.hasArg("authkey") ? server.arg("authkey").toInt() : 0;

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(aid)) {
    h += "<p class='bad'>SelectApplication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<p>Selected AID: <b>0x" + aidHex + "</b></p>";
  h += "<p>Auth key: <b>" + String(authKey) + "</b></p>";

  h += "<p>";
  h += "<a class='btn' href='/file?aid=" + aidHex + "&id=" + String(fileId) + "&authkey=0'>Use key 0</a>";
  h += "<a class='btn' href='/file?aid=" + aidHex + "&id=" + String(fileId) + "&authkey=1'>Use key 1</a>";
  h += "<a class='btn' href='/file?aid=" + aidHex + "&id=" + String(fileId) + "&authkey=2'>Use key 2</a>";
  h += "</p>";

  if (!authenticateSelectedApp(aid, uid, uidLen, h, authKey)) {
    h += "<p class='bad'>Cannot access protected application without authentication</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DESFireFileSettings fs;

  if (!gi_PN532.GetFileSettings(fileId, &fs)) {
    h += "<p class='bad'>GetFileSettings failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<div class='card'>";
  h += "<h3>File " + String(fileId) + "</h3>";
  h += "<p>Application: 0x" + aidHex + "</p>";

  h += "<p>Type: <b>";
  h += fileTypeName((int)fs.e_FileType);
  h += "</b> (" + String((int)fs.e_FileType) + ")</p>";

  h += "<p>Comm mode: <b>";
  h += commModeName((int)fs.e_Encrypt);
  h += "</b> (" + String((int)fs.e_Encrypt) + ")</p>";

  h += "<p><b>Access rights</b></p>";
  h += "<ul>";
  h += "<li>Read: " + accessRightName((byte)fs.k_Permis.e_ReadAccess) + "</li>";
  h += "<li>Write: " + accessRightName((byte)fs.k_Permis.e_WriteAccess) + "</li>";
  h += "<li>Read & Write: " + accessRightName((byte)fs.k_Permis.e_ReadAndWriteAccess) + "</li>";
  h += "<li>Change settings: " + accessRightName((byte)fs.k_Permis.e_ChangeAccess) + "</li>";
  h += "</ul>";

  h += "<form method='POST' action='/changefilesettings'>";
  h += "<input type='hidden' name='aid' value='" + aidHex + "'>";
  h += "<input type='hidden' name='id' value='" + String(fileId) + "'>";

  h += "<p><b>Change access rights</b></p>";

  h += "<p>Auth key number:</p>";
  h += "<input name='authkey' value='" + String(authKey) + "' style='width:100%;padding:8px'>";

  h += "<p>Read access (0-13 key, 14 free, 15 never):</p>";
  h += "<input name='readkey' value='" + String((int)fs.k_Permis.e_ReadAccess) + "' style='width:100%;padding:8px'>";

  h += "<p>Write access (0-13 key, 14 free, 15 never):</p>";
  h += "<input name='writekey' value='" + String((int)fs.k_Permis.e_WriteAccess) + "' style='width:100%;padding:8px'>";

  h += "<p>Read&Write access (0-13 key, 14 free, 15 never):</p>";
  h += "<input name='rwkey' value='" + String((int)fs.k_Permis.e_ReadAndWriteAccess) + "' style='width:100%;padding:8px'>";

  h += "<p>Change settings access (0-13 key, 14 free, 15 never):</p>";
  h += "<input name='changekey' value='" + String((int)fs.k_Permis.e_ChangeAccess) + "' style='width:100%;padding:8px'>";

  h += "<p>Type yes to confirm:</p>";
  h += "<input name='confirm' value='' style='width:100%;padding:8px'>";

  h += "<p><button class='btn' type='submit'>Change file settings</button></p>";
  h += "</form>";

  if (fs.e_FileType == MDFT_STANDARD_DATA_FILE ||
      fs.e_FileType == MDFT_BACKUP_DATA_FILE) {

    h += "<p>Size: " + String((unsigned long)fs.u32_FileSize) + " bytes</p>";

    if (fs.u32_FileSize > 0) {
      byte buf[128];
      uint32_t len = fs.u32_FileSize;
      if (len > sizeof(buf)) len = sizeof(buf);

      if (gi_PN532.ReadFileData(fileId, 0, len, buf)) {
        h += "<p><b>First " + String(len) + " bytes HEX:</b></p>";
        h += "<pre>";
        h += hexBuf(buf, len);
        h += "</pre>";

        h += "<p><b>First " + String(len) + " bytes ASCII:</b></p>";
        h += "<pre>";
        h += asciiFromBuf(buf, len);
        h += "</pre>";

        h += "<form method='POST' action='/writefile'>";
        h += "<input type='hidden' name='aid' value='" + aidHex + "'>";
        h += "<input type='hidden' name='id' value='" + String(fileId) + "'>";
        h += "<input type='hidden' name='authkey' value='" + String(authKey) + "'>";
        h += "<p>Edit HEX data:</p>";
        h += "<textarea name='hex' rows='5' style='width:100%;background:#080808;color:#eee;border:1px solid #555;border-radius:8px;padding:8px'>";
        h += hexBuf(buf, len);
        h += "</textarea>";
        h += "<p><button class='btn' type='submit'>Write HEX from offset 0</button></p>";
        h += "</form>";

        h += "<form method='POST' action='/writefileascii'>";
        h += "<input type='hidden' name='aid' value='" + aidHex + "'>";
        h += "<input type='hidden' name='id' value='" + String(fileId) + "'>";
        h += "<input type='hidden' name='authkey' value='" + String(authKey) + "'>";
        h += "<p>Edit ASCII data:</p>";
        h += "<textarea name='ascii' rows='5' style='width:100%;background:#080808;color:#eee;border:1px solid #555;border-radius:8px;padding:8px'>";
        h += asciiFromBuf(buf, len);
        h += "</textarea>";
        h += "<p><button class='btn' type='submit'>Write ASCII from offset 0</button></p>";
        h += "</form>";

      } else {
        h += "<p class='bad'>ReadFileData failed</p>";
      }
    }
  }

  h += "</div>";

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleApps() {
  String h = pageHeader("Applications");

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  gi_PN532.SelectApplication(0x000000);

  uint32_t aids[28];
  byte appCount = 0;

  if (!gi_PN532.GetApplicationIDs(aids, &appCount)) {
    h += "<p class='bad'>GetApplicationIDs failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<div class='card'><h3>Applications: " + String(appCount) + "</h3>";

  if (appCount == 0) {
    h += "<p>No applications</p>";
  }

  for (byte i = 0; i < appCount; i++) {
    String a = aidToHex(aids[i]);
    h += "<p>AID " + String(i) + ": <a href='/app?aid=" + a + "'>0x" + a + "</a> ";
    h += "<a href='/files?aid=" + a + "'>[files]</a></p>";
  }

  h += "</div>";

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}





bool GenerateDesfireSecrets(kUser* pk_User, DES* pi_AppMasterKey, byte u8_StoreValue[16])
{
  byte u8_Data[24] = {0};

  // Copy 7-byte UID
  memcpy(u8_Data, pk_User->ID.u8, 7);

  // XOR username + random EEPROM bytes over first 16 bytes
  int B = 0;
  for (int N = 0; N < NAME_BUF_SIZE; N++) {
    u8_Data[B++] ^= pk_User->s8_Name[N];
    if (B > 15) B = 0;
  }

  byte u8_AppMasterKey[24];

  DES i_3KDes;

  if (!i_3KDes.SetKeyData(SECRET_APPLICATION_KEY,
                          sizeof(SECRET_APPLICATION_KEY),
                          0)) {
    return false;
  }

  if (!i_3KDes.CryptDataCBC(CBC_SEND,
                            KEY_ENCIPHER,
                            u8_AppMasterKey,
                            u8_Data,
                            24)) {
    return false;
  }

  if (!i_3KDes.SetKeyData(SECRET_STORE_VALUE_KEY,
                          sizeof(SECRET_STORE_VALUE_KEY),
                          0)) {
    return false;
  }

  if (!i_3KDes.CryptDataCBC(CBC_SEND,
                            KEY_ENCIPHER,
                            u8_StoreValue,
                            u8_Data,
                            16)) {
    return false;
  }

  if (!pi_AppMasterKey->SetKeyData(u8_AppMasterKey,
                                   sizeof(u8_AppMasterKey),
                                   CARD_KEY_VERSION)) {
    return false;
  }

  return true;
}




bool tryReadFileWithStoredKeys(uint32_t aid, byte fileId, uint32_t len, byte *buf, byte uid[8], byte uidLen, String &h, byte &usedKey) {
  for (byte keyNo = 0; keyNo <= 13; keyNo++) {
    if (findStoredKey(aid, keyNo) < 0 && keyNo != 0) continue;

    if (!gi_PN532.SelectApplication(aid)) continue;

    if (!authenticateSelectedApp(aid, uid, uidLen, h, keyNo)) continue;

    if (gi_PN532.ReadFileData(fileId, 0, len, buf)) {
      usedKey = keyNo;
      return true;
    }
  }

  return false;
}

void handleDump() {
  String h = pageHeader("Full DESFire dump");

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<div class='card'>";
  h += "<h3>Card</h3>";
  h += "<p>UID: <b>" + hexBuf(uid, uidLen) + "</b></p>";
  h += "<p>UID length: " + String(uidLen) + "</p>";
  h += "<p>Card type: <b>";
  h += cardTypeName((int)cardType);
  h += "</b> (" + String((int)cardType) + ")</p>";
  h += "</div>";

  if (!gi_PN532.SelectApplication(0x000000)) {
    h += "<p class='bad'>Select PICC/root failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DESFireCardVersion ver;
  if (gi_PN532.GetCardVersion(&ver)) {
    h += "<div class='card'>";
    h += "<h3>DESFire version</h3>";
    h += "<p>HW vendor/type/subtype: " + String(ver.hardwareVendorId) + "/" + String(ver.hardwareType) + "/" + String(ver.hardwareSubType) + "</p>";
    h += "<p>HW version: " + String(ver.hardwareMajVersion) + "." + String(ver.hardwareMinVersion) + "</p>";
    h += "<p>SW vendor/type/subtype: " + String(ver.softwareVendorId) + "/" + String(ver.softwareType) + "/" + String(ver.softwareSubType) + "</p>";
    h += "<p>SW version: " + String(ver.softwareMajVersion) + "." + String(ver.softwareMinVersion) + "</p>";
    h += "<p>Storage size byte: 0x" + hexByte(ver.hardwareStorageSize) + "</p>";
    h += "<p>Production week/year: " + hexByte(ver.cwProd) + "/" + hexByte(ver.yearProd) + "</p>";
    h += "</div>";
  } else {
    h += "<p class='bad'>GetCardVersion failed</p>";
  }

  uint32_t freeMem = 0;
  if (gi_PN532.GetFreeMemory(&freeMem)) {
    h += "<p>Free memory: <b>" + String(freeMem) + "</b> bytes</p>";
  } else {
    h += "<p class='bad'>GetFreeMemory failed</p>";
  }

  DESFireKeySettings piccKs;
  byte piccKeyCount = 0;
  DESFireKeyType piccKeyType;

  if (gi_PN532.GetKeySettings(&piccKs, &piccKeyCount, &piccKeyType)) {
    h += "<div class='card'>";
    h += "<h3>PICC/root</h3>";
    h += "<p>Key settings: 0x" + hexByte((byte)piccKs) + "</p>";
    h += "<p>Key count: " + String(piccKeyCount) + "</p>";
    h += "<p>Key type: <b>";
    h += keyTypeName((int)piccKeyType);
    h += "</b> (" + String((int)piccKeyType) + ")</p>";
    h += "</div>";
  } else {
    h += "<p class='bad'>PICC GetKeySettings failed</p>";
  }

  uint32_t aids[28];
  byte appCount = 0;

  if (!gi_PN532.GetApplicationIDs(aids, &appCount)) {
    h += "<p class='bad'>GetApplicationIDs failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<div class='card'>";
  h += "<h3>Applications: " + String(appCount) + "</h3>";
  h += "</div>";

  for (byte a = 0; a < appCount; a++) {
    uint32_t aid = aids[a];
    String aidHex = aidToHex(aid);

    h += "<div class='card'>";
    h += "<h3>AID " + String(a) + ": 0x" + aidHex + "</h3>";
    h += "<p><a href='/app?aid=" + aidHex + "'>open app page</a> ";
    h += "<a href='/files?aid=" + aidHex + "'>[files]</a></p>";

    if (!gi_PN532.SelectApplication(aid)) {
      h += "<p class='bad'>SelectApplication failed</p>";
      h += "</div>";
      continue;
    }

    if (!authenticateSelectedApp(aid, uid, uidLen, h)) {
      h += "<p class='bad'>Authentication failed, skipping protected app contents</p>";
      h += "</div>";
      continue;
    }

    DESFireKeySettings ks;
    byte keyCount = 0;
    DESFireKeyType keyType;

    if (gi_PN532.GetKeySettings(&ks, &keyCount, &keyType)) {
      h += "<p>App key settings: 0x" + hexByte((byte)ks) + "</p>";
      h += "<p>App key count: " + String(keyCount) + "</p>";
      h += "<p>App key type: <b>";
      h += keyTypeName((int)keyType);
      h += "</b> (" + String((int)keyType) + ")</p>";
    } else {
      h += "<p class='bad'>App GetKeySettings failed</p>";
    }

    byte fileIds[16] = {0};
    byte fileCount = 0;

    if (!gi_PN532.GetFileIDs(fileIds, &fileCount)) {
      h += "<p class='bad'>GetFileIDs failed</p>";
      h += "</div>";
      continue;
    }

    h += "<h4>Files: " + String(fileCount) + "</h4>";

    for (byte f = 0; f < fileCount; f++) {
      byte fileId = fileIds[f];

      DESFireFileSettings fs;

      h += "<div class='card'>";
      h += "<h4>File " + String(fileId) + "</h4>";

      if (!gi_PN532.GetFileSettings(fileId, &fs)) {
        h += "<p class='bad'>GetFileSettings failed</p>";
        h += "</div>";
        continue;
      }

      h += "<p>Type: <b>";
      h += fileTypeName((int)fs.e_FileType);
      h += "</b> (" + String((int)fs.e_FileType) + ")</p>";

      h += "<p>Comm mode: <b>";
      h += commModeName((int)fs.e_Encrypt);
      h += "</b> (" + String((int)fs.e_Encrypt) + ")</p>";

      h += "<p><b>Access rights</b></p>";
      h += "<ul>";
      h += "<li>Read: " + accessRightName((byte)fs.k_Permis.e_ReadAccess) + "</li>";
      h += "<li>Write: " + accessRightName((byte)fs.k_Permis.e_WriteAccess) + "</li>";
      h += "<li>Read & Write: " + accessRightName((byte)fs.k_Permis.e_ReadAndWriteAccess) + "</li>";
      h += "<li>Change settings: " + accessRightName((byte)fs.k_Permis.e_ChangeAccess) + "</li>";
      h += "</ul>";

      if (fs.e_FileType == MDFT_STANDARD_DATA_FILE ||
          fs.e_FileType == MDFT_BACKUP_DATA_FILE) {

        h += "<p>Size: " + String((unsigned long)fs.u32_FileSize) + " bytes</p>";

        if (fs.u32_FileSize > 0) {
          byte buf[128];
          uint32_t len = fs.u32_FileSize;
          if (len > sizeof(buf)) len = sizeof(buf);

byte usedKey = 255;

if (tryReadFileWithStoredKeys(aid, fileId, len, buf, uid, uidLen, h, usedKey)) {
  h += "<p class='ok'>Read using key " + String(usedKey) + "</p>";
  h += "<p><b>First " + String(len) + " bytes HEX:</b></p>";
  h += "<pre>" + hexBuf(buf, len) + "</pre>";
  h += "<p><b>ASCII:</b></p>";
  h += "<pre>" + asciiFromBuf(buf, len) + "</pre>";
} else {
  h += "<p class='bad'>ReadFileData failed with all stored keys</p>";
}
        }
      }

      h += "</div>";
    }

    h += "</div>";
  }

  releaseCard();

  h += pageFooter();
  server.send(200, "text/html", h);
}


void handleCreateAppPage() {
  String h = pageHeader("Create application");

  h += "<div class='card'>";
  h += "<form method='POST' action='/createapp'>";
  h += "<p>AID, 6 hex chars:</p>";
  h += "<input name='aid' value='AA4020' style='width:100%;padding:8px'>";

  h += "<p>Key count, 1..14:</p>";
  h += "<input name='keycount' value='2' style='width:100%;padding:8px'>";

  h += "<p>Creates DES/3K3DES app with selectable key count and factory key settings.</p>";
  h += "<p><button class='btn' type='submit'>Create application</button></p>";
  h += "</form>";
  h += "</div>";

  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleCreateAppPost() {
  String h = pageHeader("Create application");

  if (!server.hasArg("aid") || !server.hasArg("keycount")) {
    h += "<p class='bad'>Missing aid or keycount</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);
  byte keyCount = server.arg("keycount").toInt();

  if (keyCount < 1 || keyCount > 14) {
    h += "<p class='bad'>Key count must be 1..14</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(0x000000)) {
    h += "<p class='bad'>Select PICC/root failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.Authenticate(0, &gi_PN532.DES2_DEFAULT_KEY)) {
    h += "<p class='bad'>PICC default DES key authentication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  h += "<p>Creating AID 0x" + aidHex + " with key count " + String(keyCount) + "</p>";

  if (!gi_PN532.CreateApplication(aid, KS_FACTORY_DEFAULT, keyCount, DF_KEY_3K3DES)) {
    h += "<p class='bad'>CreateApplication failed</p>";
  } else {
    h += "<p class='ok'>Application created: 0x" + aidHex + "</p>";
    h += "<p>Key count: " + String(keyCount) + "</p>";
    h += "<p><a class='btn' href='/app?aid=" + aidHex + "'>Open app</a></p>";
  }

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleCreateFilePage() {
  String h = pageHeader("Create standard file");

  String aid = server.hasArg("aid") ? server.arg("aid") : "AA4020";

  h += "<div class='card'>";
  h += "<form method='POST' action='/createfile'>";
  h += "<p>AID:</p>";
  h += "<input name='aid' value='" + aid + "' style='width:100%;padding:8px'>";
  h += "<p>Read access (0-13 key, 14 free, 15 never):</p>";
  h += "<input name='readkey' value='0' style='width:100%;padding:8px'>";

  h += "<p>Write access (0-13 key, 14 free, 15 never):</p>";
  h += "<input name='writekey' value='0' style='width:100%;padding:8px'>";

  h += "<p>Read&Write access (0-13 key, 14 free, 15 never):</p>";
  h += "<input name='rwkey' value='0' style='width:100%;padding:8px'>";

  h += "<p>Change settings access (0-13 key, 14 free, 15 never):</p>";
  h += "<input name='changekey' value='0' style='width:100%;padding:8px'>";
  h += "<p>File ID:</p>";
  h += "<input name='fileid' value='0' style='width:100%;padding:8px'>";
  h += "<p>Size bytes:</p>";
  h += "<input name='size' value='16' style='width:100%;padding:8px'>";
  h += "<p>Creates standard data file, plain comms, key0 full access.</p>";
  h += "<p><button class='btn' type='submit'>Create file</button></p>";
  h += "</form>";
  h += "</div>";

  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleCreateFilePost() {
  String h = pageHeader("Create standard file");

  if (!server.hasArg("aid") || !server.hasArg("fileid") || !server.hasArg("size")) {
    h += "<p class='bad'>Missing aid, fileid, or size</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);
  byte fileId = server.arg("fileid").toInt();
  uint32_t size = server.arg("size").toInt();

  if (fileId > 31 || size == 0 || size > 1024) {
    h += "<p class='bad'>Invalid file id or size. Use file id 0..31 and size 1..1024.</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(aid)) {
    h += "<p class='bad'>SelectApplication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!authenticateSelectedApp(aid, uid, uidLen, h)) {
    h += "<p class='bad'>Authentication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

byte readKey   = server.hasArg("readkey")   ? server.arg("readkey").toInt()   : 0;
byte writeKey  = server.hasArg("writekey")  ? server.arg("writekey").toInt()  : 0;
byte rwKey     = server.hasArg("rwkey")     ? server.arg("rwkey").toInt()     : 0;
byte changeKey = server.hasArg("changekey") ? server.arg("changekey").toInt() : 0;

if (readKey > 15 || writeKey > 15 || rwKey > 15 || changeKey > 15) {
  h += "<p class='bad'>Access values must be 0..15</p>";
  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
  return;
}

DESFireFilePermissions perm;
perm.e_ReadAccess         = (DESFireAccessRights)readKey;
perm.e_WriteAccess        = (DESFireAccessRights)writeKey;
perm.e_ReadAndWriteAccess = (DESFireAccessRights)rwKey;
perm.e_ChangeAccess       = (DESFireAccessRights)changeKey;

  if (!gi_PN532.CreateStdDataFile(fileId, &perm, size)) {
  h += "<p class='bad'>CreateStdDataFile failed</p>";
  h += "<p>Possible causes:</p>";
  h += "<ul>";
  h += "<li>File ID already exists</li>";
  h += "<li>Application key settings do not allow file creation</li>";
  h += "<li>Not enough free memory</li>";
  h += "<li>Wrong authentication key</li>";
  h += "</ul>";
  }

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleDeleteAppPage() {
  String h = pageHeader("Delete application");

  String aid = server.hasArg("aid") ? server.arg("aid") : "AA4020";

  h += "<div class='card'>";
  h += "<p class='bad'><b>Danger:</b> deletes the complete application and all files inside it.</p>";
  h += "<form method='POST' action='/deleteapp'>";
  h += "<p>AID:</p>";
  h += "<input name='aid' value='" + aid + "' style='width:100%;padding:8px'>";
  h += "<p>Type yes to confirm:</p>";
  h += "<input name='confirm' value='' style='width:100%;padding:8px'>";
  h += "<p><button class='btn' type='submit'>Delete application</button></p>";
  h += "</form>";
  h += "</div>";

  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleDeleteAppPost() {
  String h = pageHeader("Delete application");

  if (!server.hasArg("aid") || !server.hasArg("confirm")) {
    h += "<p class='bad'>Missing aid or confirm</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  String confirm = server.arg("confirm");
  confirm.toLowerCase();

  if (confirm != "yes") {
    h += "<p class='bad'>Refused. confirm must be exactly yes.</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(0x000000)) {
    h += "<p class='bad'>Select PICC/root failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.Authenticate(0, &gi_PN532.DES2_DEFAULT_KEY)) {
    h += "<p class='bad'>PICC default DES key authentication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.DeleteApplication(aid)) {
    h += "<p class='bad'>DeleteApplication failed</p>";
  } else {
    h += "<p class='ok'>Deleted application 0x" + aidHex + "</p>";
    h += "<p><a class='btn' href='/apps'>Back to apps</a></p>";
  }

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleListKeys() {
  String h = pageHeader("Stored app keys");

  h += "<div class='card'>";
  h += "<h3>ESP32 stored DES app keys</h3>";

  bool any = false;

  for (int i = 0; i < KEYSTORE_MAX; i++) {
    if (!keyStore[i].used) continue;

    any = true;
    String aidHex = aidToHex(keyStore[i].aid);

    h += "<p>Slot ";
    h += String(i);
    h += ": AID 0x";
    h += aidHex;
    h += " key ";
    h += String(keyStore[i].keyNo);
    h += " version ";
    h += String(keyStore[i].version);

    h += " <a href='/delkey?aid=";
    h += aidHex;
    h += "&keyno=";
    h += String(keyStore[i].keyNo);
    h += "'>[delete]</a></p>";
  }

  if (!any) h += "<p>No stored keys.</p>";

  h += "</div>";
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleChangeAppKeyPage() {
  String h = pageHeader("Change app key");

  String aid = server.hasArg("aid") ? server.arg("aid") : "AA4020";

  h += "<div class='card'>";
  h += "<p class='bad'><b>Warning:</b> this changes a DESFire application key.</p>";

  h += "<form method='POST' action='/changeappkey'>";

  h += "<p>AID:</p>";
  h += "<input name='aid' value='" + aid + "' style='width:100%;padding:8px'>";

  h += "<p>Key number (0-13):</p>";
  h += "<input name='keyno' value='0' style='width:100%;padding:8px'>";

  h += "<p>New 24-byte DES/3K3DES key, 48 hex chars:</p>";
  h += "<textarea name='key' rows='3' style='width:100%;background:#080808;color:#eee;border:1px solid #555;border-radius:8px;padding:8px'>00112233445566778899AABBCCDDEEFF0011223344556677</textarea>";

  h += "<p>Key version:</p>";
  h += "<input name='version' value='1' style='width:100%;padding:8px'>";

  h += "<p>Type yes to confirm:</p>";
  h += "<input name='confirm' value='' style='width:100%;padding:8px'>";

  h += "<p><button class='btn' type='submit'>Change / add key</button></p>";

  h += "</form>";
  h += "</div>";

  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleChangeAppKeyPost() {
  String h = pageHeader("Change app key");

  if (!server.hasArg("aid") || !server.hasArg("keyno") ||
      !server.hasArg("key") || !server.hasArg("version") ||
      !server.hasArg("confirm")) {
    h += "<p class='bad'>Missing aid, keyno, key, version, or confirm</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  String confirm = server.arg("confirm");
  confirm.toLowerCase();

  if (confirm != "yes") {
    h += "<p class='bad'>Refused. confirm must be exactly yes.</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);

  byte keyNo = server.arg("keyno").toInt();
  if (keyNo > 13) {
    h += "<p class='bad'>Key number must be 0..13</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  byte newKeyBytes[24];
  int keyLen = 0;

  if (!parseHexToBytes(server.arg("key"), newKeyBytes, 24, keyLen) || keyLen != 24) {
    h += "<p class='bad'>Key must be exactly 24 bytes / 48 hex chars.</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  byte version = server.arg("version").toInt();

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(aid)) {
    h += "<p class='bad'>SelectApplication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  // For changing key 0: authenticate with key 0.
  // For changing key 1..13 under factory settings: authenticate with key 0.
  if (!authenticateSelectedApp(aid, uid, uidLen, h, 0)) {
    h += "<p class='bad'>Authentication failed. Cannot change key.</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DES newKey;
  if (!newKey.SetKeyData(newKeyBytes, 24, version)) {
    h += "<p class='bad'>New key setup failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  bool ok;

  if (keyNo == 0) {
    ok = gi_PN532.ChangeKey(0, &newKey, NULL);
  } else {
      // Existing untouched extra keys are factory default DES key.
    ok = gi_PN532.ChangeKey(keyNo, &newKey, &gi_PN532.DES2_DEFAULT_KEY);
  }

  if (!ok) {
    h += "<p class='bad'>ChangeKey failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!storeAppKey(aid, keyNo, newKeyBytes, version)) {
    h += "<p class='bad'>Card key changed, but ESP32 key store is full / save failed!</p>";
  } else {
    h += "<p class='ok'>App key ";
    h += String(keyNo);
    h += " changed and stored on ESP32</p>";
  }

  h += "<p>AID: 0x" + aidHex + "</p>";
  h += "<p>Key number: " + String(keyNo) + "</p>";
  h += "<p><a class='btn' href='/app?aid=" + aidHex + "'>Open app</a></p>";
  h += "<p><a class='btn' href='/listkeys'>Stored keys</a></p>";

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleDeleteKeyPage() {
  String h = pageHeader("Delete stored key");

  String aid = server.hasArg("aid") ? server.arg("aid") : "AA4020";
  String keyno = server.hasArg("keyno") ? server.arg("keyno") : "0";

  h += "<div class='card'>";
  h += "<p>This only deletes the key from ESP32 storage. It does not change the card.</p>";
  h += "<form method='POST' action='/delkey'>";

  h += "<p>AID:</p>";
  h += "<input name='aid' value='" + aid + "' style='width:100%;padding:8px'>";

  h += "<p>Key number:</p>";
  h += "<input name='keyno' value='" + keyno + "' style='width:100%;padding:8px'>";

  h += "<p>Type yes to confirm:</p>";
  h += "<input name='confirm' value='' style='width:100%;padding:8px'>";

  h += "<p><button class='btn' type='submit'>Delete stored key</button></p>";
  h += "</form>";
  h += "</div>";

  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleDeleteKeyPost() {
  String h = pageHeader("Delete stored key");

  if (!server.hasArg("aid") || !server.hasArg("keyno") || !server.hasArg("confirm")) {
    h += "<p class='bad'>Missing aid, keyno, or confirm</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  String confirm = server.arg("confirm");
  confirm.toLowerCase();

  if (confirm != "yes") {
    h += "<p class='bad'>Refused. confirm must be exactly yes.</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);
  byte keyNo = server.arg("keyno").toInt();

  int slot = findStoredKey(aid, keyNo);

  if (slot < 0) {
    h += "<p class='bad'>No stored key for AID 0x" + aidHex + " key " + String(keyNo) + "</p>";
  } else {
    memset(&keyStore[slot], 0, sizeof(StoredAppKey));
    keyStore[slot].magic = KEYSTORE_MAGIC;
    saveKeyStoreSlot(slot);

    h += "<p class='ok'>Stored key deleted for AID 0x";
    h += aidHex;
    h += " key ";
    h += String(keyNo);
    h += "</p>";
  }

  h += "<p><a class='btn' href='/listkeys'>Back to stored keys</a></p>";
  h += pageFooter();
  server.send(200, "text/html", h);
}

void handleChangeFileSettingsPost() {
  String h = pageHeader("Change file settings");

  if (!server.hasArg("aid") || !server.hasArg("id") ||
      !server.hasArg("readkey") || !server.hasArg("writekey") ||
      !server.hasArg("rwkey") || !server.hasArg("changekey") ||
      !server.hasArg("confirm")) {
    h += "<p class='bad'>Missing parameter</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  String confirm = server.arg("confirm");
  confirm.toLowerCase();

  if (confirm != "yes") {
    h += "<p class='bad'>Refused. confirm must be exactly yes.</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  uint32_t aid = parseAid(server.arg("aid"));
  String aidHex = aidToHex(aid);
  byte fileId = server.arg("id").toInt();
  byte authKey = server.hasArg("authkey") ? server.arg("authkey").toInt() : 0;

  byte readKey   = server.arg("readkey").toInt();
  byte writeKey  = server.arg("writekey").toInt();
  byte rwKey     = server.arg("rwkey").toInt();
  byte changeKey = server.arg("changekey").toInt();

  if (readKey > 15 || writeKey > 15 || rwKey > 15 || changeKey > 15) {
    h += "<p class='bad'>Access values must be 0..15</p>";
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  byte uid[8];
  byte uidLen = 0;
  eCardType cardType;

  if (!readCard(uid, &uidLen, &cardType, h)) {
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!gi_PN532.SelectApplication(aid)) {
    h += "<p class='bad'>SelectApplication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  if (!authenticateSelectedApp(aid, uid, uidLen, h, authKey)) {
    h += "<p class='bad'>Authentication failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DESFireFileSettings fs;
  if (!gi_PN532.GetFileSettings(fileId, &fs)) {
    h += "<p class='bad'>GetFileSettings failed</p>";
    releaseCard();
    h += pageFooter();
    server.send(200, "text/html", h);
    return;
  }

  DESFireFilePermissions perm;
  perm.e_ReadAccess         = (DESFireAccessRights)readKey;
  perm.e_WriteAccess        = (DESFireAccessRights)writeKey;
  perm.e_ReadAndWriteAccess = (DESFireAccessRights)rwKey;
  perm.e_ChangeAccess       = (DESFireAccessRights)changeKey;

  if (!gi_PN532.ChangeFileSettings(fileId, fs.e_Encrypt, &perm)) {
    h += "<p class='bad'>ChangeFileSettings failed</p>";
  } else {
    h += "<p class='ok'>File settings changed</p>";
    h += "<p>Read: " + accessRightName(readKey) + "</p>";
    h += "<p>Write: " + accessRightName(writeKey) + "</p>";
    h += "<p>Read & Write: " + accessRightName(rwKey) + "</p>";
    h += "<p>Change settings: " + accessRightName(changeKey) + "</p>";
  }

  h += "<p><a class='btn' href='/file?aid=" + aidHex + "&id=" + String(fileId) + "&authkey=" + String(authKey) + "'>Back to file</a></p>";

  releaseCard();
  h += pageFooter();
  server.send(200, "text/html", h);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("ESP32 DESFire Web Workbench V001 - DES read-only");

  gi_PN532.InitHardwareSPI(PN532_CS_PIN, PN532_RESET_PIN);
  gi_PN532.begin();
  EEPROM.begin(2048);
  loadKeyStore();

  byte IC, vh, vl, flags;
  if (gi_PN532.GetFirmwareVersion(&IC, &vh, &vl, &flags)) {
    Serial.print("PN532 firmware ");
    Serial.print(vh);
    Serial.print(".");
    Serial.println(vl);
  } else {
    Serial.println("PN532 not found");
  }

  gi_PN532.SetPassiveActivationRetries();
  gi_PN532.SamConfig();

WiFi.mode(WIFI_STA);
WiFi.begin(WIFI_SSID, WIFI_PASS);

Serial.print("Connecting to WiFi");

while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
}

Serial.println();
Serial.print("Connected to ");
Serial.println(WIFI_SSID);

Serial.print("Open: http://");
Serial.println(WiFi.localIP());

  server.on("/", handleHome);
  server.on("/reader", handleReader);
  server.on("/card", handleCard);
  server.on("/picc", handlePicc);
  server.on("/apps", handleApps);
  server.on("/app", handleApp);
  server.on("/files", handleFiles);
  server.on("/file", handleFile);
  server.on("/dump", handleDump);
  server.on("/writefile", HTTP_POST, handleWriteFile);
  server.on("/createapp", HTTP_GET, handleCreateAppPage);
  server.on("/createapp", HTTP_POST, handleCreateAppPost);
  server.on("/createfile", HTTP_GET, handleCreateFilePage);
  server.on("/createfile", HTTP_POST, handleCreateFilePost);
  server.on("/deleteapp", HTTP_GET, handleDeleteAppPage);
  server.on("/deleteapp", HTTP_POST, handleDeleteAppPost);
  server.on("/listkeys", handleListKeys);
  server.on("/changeappkey", HTTP_GET, handleChangeAppKeyPage);
  server.on("/changeappkey", HTTP_POST, handleChangeAppKeyPost);
  server.on("/delkey", HTTP_GET, handleDeleteKeyPage);
  server.on("/delkey", HTTP_POST, handleDeleteKeyPost);
  server.on("/writefileascii", HTTP_POST, handleWriteFileAscii);
  server.on("/changefilesettings", HTTP_POST, handleChangeFileSettingsPost);

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
