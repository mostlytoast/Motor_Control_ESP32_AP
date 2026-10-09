/*
  ESP32 + merged PN532 library + NTAG215 NDEF Web Writer/Dumper V005

  Web functions:
    /        main page
    /write   write text input as NDEF URI record
    /clear   clear NDEF/user pages and restore CC bytes
    /dump    dump pages 0..134 as hex/ASCII and decode simple NDEF URI

  PN532 SPI wiring:
    SCK  = 18
    MISO = 19
    MOSI = 23
    SS   = 5
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include "PN532.h"
#include <secret.h>

// ================= PN532 SPI =================
#define PN532_SCK   18
#define PN532_MISO  19
#define PN532_MOSI  23
#define PN532_SS    5
#define PN532_RESET 255   // RST not connected

PN532 nfc;

WebServer server(80);

// ================= NTAG215 LAYOUT =================
static const uint8_t NTAG215_FIRST_USER_PAGE = 4;
static const uint8_t NTAG215_LAST_NDEF_PAGE  = 129;
static const uint8_t NTAG215_LAST_DUMP_PAGE  = 134;
static const uint16_t NTAG215_USER_BYTES =
  (NTAG215_LAST_NDEF_PAGE - NTAG215_FIRST_USER_PAGE + 1) * 4;

String lastMessage = "";
String lastDump = "";
String currentUid = "";

// ---------- Helpers ----------
String htmlEscape(const String &s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else out += c;
  }
  return out;
}

String urlDecode(const String &src) {
  String ret;
  ret.reserve(src.length());
  char hex[3] = {0, 0, 0};

  for (uint16_t i = 0; i < src.length(); i++) {
    char c = src[i];

    if (c == '+') {
      ret += ' ';
    } else if (c == '%' && i + 2 < src.length()) {
      hex[0] = src[i + 1];
      hex[1] = src[i + 2];
      ret += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      ret += c;
    }
  }

  return ret;
}

void appendHexByte(String &s, uint8_t b) {
  if (b < 0x10) s += '0';
  s += String(b, HEX);
}

String uidToString(uint8_t *uid, uint8_t uidLen) {
  String s = "";
  for (uint8_t i = 0; i < uidLen; i++) {
    if (uid[i] < 0x10) s += '0';
    s += String(uid[i], HEX);
    if (i + 1 < uidLen) s += ':';
  }
  return s;
}

bool selectTag(String &msg) {
  uint8_t uid[PN532_MAX_UID_LEN];
  uint8_t uidLen = 0;
  eCardType cardType = CARD_Unknown;

  // Merged-library behaviour:
  //   false            = PN532 communication error
  //   true, uidLen == 0 = no ISO14443A tag present
  //   true, uidLen > 0  = tag selected
  bool ok = nfc.ReadPassiveTargetID(uid, &uidLen, &cardType);

  if (!ok) {
    msg = "PN532 communication error.";
    return false;
  }

  if (uidLen == 0) {
    msg = "No tag found.";
    return false;
  }

  currentUid = uidToString(uid, uidLen);

  msg = "Tag selected. UID=";
  msg += currentUid;
  msg += " UID length=";
  msg += String(uidLen);

  return true;
}

// Local NTAG/Ultralight READ helper.
// Command 0x30 reads four consecutive pages (16 bytes).
bool ntagRead16Local(uint8_t startPage, uint8_t out16[16]) {
  if (out16 == nullptr) return false;

  uint8_t cmd[2] = {0x30, startPage};
  uint8_t rx[20];
  uint8_t rxLen = 0;

  if (!nfc.dataExchange(cmd, sizeof(cmd), rx, &rxLen, sizeof(rx))) {
    return false;
  }

  if (rxLen < 16) return false;

  memcpy(out16, rx, 16);
  return true;
}

// Local NTAG/Ultralight WRITE helper.
// Command 0xA2 writes exactly one four-byte page.
bool ntagWritePageLocal(uint8_t page, const uint8_t data4[4]) {
  if (data4 == nullptr) return false;

  uint8_t cmd[6] = {
    0xA2,
    page,
    data4[0],
    data4[1],
    data4[2],
    data4[3]
  };

  uint8_t rx[4];
  uint8_t rxLen = 0;

  // dataExchange() validates the PN532 response/status.
  if (!nfc.dataExchange(cmd, sizeof(cmd), rx, &rxLen, sizeof(rx))) {
    return false;
  }

  // Allow the NTAG EEPROM write cycle to complete.
  delay(8);
  return true;
}

bool readPage(uint8_t page, uint8_t out4[4], String &err) {
  uint8_t block[16];

  if (!ntagRead16Local(page, block)) {
    err = "Read failed at page " + String(page);
    return false;
  }

  memcpy(out4, block, 4);
  return true;
}

bool writePage(uint8_t page, const uint8_t data[4], String &err) {
  if (!ntagWritePageLocal(page, data)) {
    err = "Write failed at page " + String(page);
    return false;
  }

  return true;
}

// Choose a standard NDEF URI prefix code and strip that prefix from the stored text.
uint8_t chooseUriPrefix(String &uri) {
  if (uri.startsWith("http://www."))  { uri.remove(0, 11); return 0x01; }
  if (uri.startsWith("https://www.")) { uri.remove(0, 12); return 0x02; }
  if (uri.startsWith("http://"))      { uri.remove(0, 7);  return 0x03; }
  if (uri.startsWith("https://"))     { uri.remove(0, 8);  return 0x04; }

  // For spotify:track:... or plain open.spotify.com/track/... use no prefix abbreviation.
  return 0x00;
}

String prefixToString(uint8_t code) {
  switch (code) {
    case 0x01: return "http://www.";
    case 0x02: return "https://www.";
    case 0x03: return "http://";
    case 0x04: return "https://";
    default: return "";
  }
}

bool buildNdefUri(const String &input, uint8_t *out, uint16_t &outLen, String &err) {
  String uri = input;
  uri.trim();

  if (uri.length() == 0) {
    err = "Input is empty.";
    return false;
  }

  uint8_t prefixCode = chooseUriPrefix(uri);
  uint16_t uriRestLen = uri.length();
  uint16_t payloadLen = 1 + uriRestLen;
  uint16_t recordLen  = 3 + 1 + payloadLen;
  uint16_t tlvLen     = recordLen;
  uint16_t totalLen   = 2 + tlvLen + 1;

  if (payloadLen > 255 || tlvLen > 254) {
    err = "URI is too long for this simple short-record writer.";
    return false;
  }

  if (totalLen > NTAG215_USER_BYTES) {
    err = "NDEF data is too large for NTAG215 user memory.";
    return false;
  }

  uint16_t i = 0;
  out[i++] = 0x03;
  out[i++] = (uint8_t)tlvLen;
  out[i++] = 0xD1;
  out[i++] = 0x01;
  out[i++] = (uint8_t)payloadLen;
  out[i++] = 0x55;
  out[i++] = prefixCode;

  for (uint16_t k = 0; k < uriRestLen; k++) {
    out[i++] = (uint8_t)uri[k];
  }

  out[i++] = 0xFE;

  outLen = i;
  return true;
}

bool writeNdefUriToTag(const String &input, String &msg) {
  if (!selectTag(msg)) return false;

  uint8_t ndef[NTAG215_USER_BYTES];
  memset(ndef, 0x00, sizeof(ndef));

  uint16_t ndefLen = 0;

  String err;
  if (!buildNdefUri(input, ndef, ndefLen, err)) {
    msg = err;
    return false;
  }

  // Capability Container for NTAG215.
  const uint8_t cc[4] = {0xE1, 0x10, 0x3E, 0x00};
  if (!writePage(3, cc, msg)) return false;

  uint16_t pagesNeeded = (ndefLen + 3) / 4;

  for (uint16_t p = 0; p < pagesNeeded; p++) {
    uint8_t pageData[4] = {0, 0, 0, 0};

    for (uint8_t k = 0; k < 4; k++) {
      uint16_t idx = p * 4 + k;
      if (idx < ndefLen) pageData[k] = ndef[idx];
    }

    if (!writePage(NTAG215_FIRST_USER_PAGE + p, pageData, msg)) return false;
  }

  // Clear remaining old data.
  const uint8_t zero[4] = {0, 0, 0, 0};

  for (uint16_t page = NTAG215_FIRST_USER_PAGE + pagesNeeded;
       page <= NTAG215_LAST_NDEF_PAGE;
       page++) {
    if (!writePage(page, zero, msg)) return false;
  }

  msg = "Wrote NDEF URI. Bytes=" + String(ndefLen) +
        ", pages=" + String(pagesNeeded) + ".";

  return true;
}

bool clearTag(String &msg) {
  if (!selectTag(msg)) return false;

  const uint8_t cc[4] = {0xE1, 0x10, 0x3E, 0x00};
  if (!writePage(3, cc, msg)) return false;

  uint8_t first[4] = {0x03, 0x00, 0xFE, 0x00};
  if (!writePage(4, first, msg)) return false;

  const uint8_t zero[4] = {0, 0, 0, 0};

  for (uint16_t page = 5; page <= NTAG215_LAST_NDEF_PAGE; page++) {
    if (!writePage(page, zero, msg)) return false;
  }

  msg = "Cleared NTAG215 NDEF/user memory. UID/manufacturer/lock/config pages were not changed.";
  return true;
}

String decodeSimpleNdefUri(const uint8_t *mem, uint16_t len) {
  uint16_t i = 0;

  while (i < len) {
    uint8_t tlv = mem[i++];

    if (tlv == 0x00) continue;
    if (tlv == 0xFE) return "No NDEF record before terminator.";

    if (tlv != 0x03) {
      return "First non-null TLV is not NDEF. TLV=0x" + String(tlv, HEX);
    }

    if (i >= len) return "Bad NDEF TLV length.";

    uint16_t ndefLen = mem[i++];

    if (ndefLen == 0xFF) {
      if (i + 1 >= len) return "Bad extended TLV length.";
      ndefLen = ((uint16_t)mem[i] << 8) | mem[i + 1];
      i += 2;
    }

    if (ndefLen == 0) return "Empty NDEF TLV.";
    if (i + ndefLen > len) return "NDEF length goes past read memory.";

    uint16_t r = i;
    uint8_t hdr = mem[r++];

    bool sr = hdr & 0x10;
    if (!sr) return "NDEF found, but not a short record.";

    if (r + 3 > i + ndefLen) return "NDEF record too short.";

    uint8_t typeLen = mem[r++];
    uint8_t payloadLen = mem[r++];

    if (typeLen != 1) return "NDEF found, but type length is not 1.";

    uint8_t type = mem[r++];

    if (type != 0x55) return "NDEF found, but record type is not URI (0x55).";
    if (payloadLen < 1) return "URI payload too short.";

    uint8_t prefix = mem[r++];

    String uri = prefixToString(prefix);

    for (uint8_t k = 1; k < payloadLen && r < i + ndefLen; k++, r++) {
      char c = (char)mem[r];
      if (c >= 32 && c <= 126) uri += c;
    }

    return uri;
  }

  return "No NDEF TLV found.";
}

bool dumpTag(String &dump, String &msg) {
  if (!selectTag(msg)) return false;

  dump = "";
  dump.reserve(7000);

  dump += "NTAG215 dump\n";
  dump += msg + "\n";
  dump += "Page    Hex bytes       ASCII\n";
  dump += "--------------------------------\n";

  uint8_t userMem[NTAG215_USER_BYTES];
  memset(userMem, 0, sizeof(userMem));

  for (uint16_t page = 0; page <= NTAG215_LAST_DUMP_PAGE; page++) {
    uint8_t b4[4];
    String err;

    if (!readPage((uint8_t)page, b4, err)) {
      dump += err + "\n";
      continue;
    }

    if (page < 100) dump += " ";
    if (page < 10) dump += " ";
    dump += String(page);
    dump += "     ";

    for (uint8_t k = 0; k < 4; k++) {
      uint8_t b = b4[k];
      appendHexByte(dump, b);
      dump += ' ';

      if (page >= NTAG215_FIRST_USER_PAGE && page <= NTAG215_LAST_NDEF_PAGE) {
        uint16_t idx = (page - NTAG215_FIRST_USER_PAGE) * 4 + k;
        userMem[idx] = b;
      }
    }

    dump += "     ";

    for (uint8_t k = 0; k < 4; k++) {
      uint8_t b = b4[k];
      if (b >= 32 && b <= 126) dump += (char)b;
      else dump += '.';
    }

    dump += "\n";
  }

  dump += "\nDecoded simple NDEF URI:\n";
  dump += decodeSimpleNdefUri(userMem, sizeof(userMem));
  dump += "\n";

  msg = "Dump complete.";
  return true;
}

String pageHtml() {
  String h;
  h.reserve(9000 + lastDump.length());

  h += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  h += F("<title>NTAG215 NDEF Writer/Dumper</title>");
  h += F("<style>");
  h += F("body{font-family:Arial,sans-serif;margin:20px;max-width:950px}");
  h += F("input{width:100%;font-size:18px;padding:8px}");
  h += F("button{font-size:18px;padding:10px 16px;margin:8px 6px 8px 0}");
  h += F("textarea{width:100%;height:460px;font-family:Consolas,monospace;font-size:14px}");
  h += F(".msg{padding:10px;background:#eee;margin:10px 0;white-space:pre-wrap}");
  h += F("</style>");
  h += F("</head><body><h2>ESP32 PN532 NTAG215 NDEF Writer/Dumper</h2>");
  h += F("<p>Hold one NTAG215 tag on the PN532, then click a button.</p>");

  h += F("<form method='POST' action='/write'>");
  h += F("<input name='text' placeholder='https://open.spotify.com/track/... or spotify:track:...' value=''>");
  h += F("<br><button type='submit'>Write input to tag</button>");
  h += F("</form>");

  h += F("<form method='POST' action='/clear' style='display:inline'>");
  h += F("<button type='submit'>Clear tag</button></form>");

  h += F("<form method='GET' action='/dump' style='display:inline'>");
  h += F("<button type='submit'>Dump tag data</button></form>");

  h += F("<div class='msg'>");
  h += htmlEscape(lastMessage);
  h += F("</div><h3>Card dump</h3><textarea readonly>");
  h += htmlEscape(lastDump);
  h += F("</textarea></body></html>");

  return h;
}

void handleRoot() {
  server.send(200, "text/html", pageHtml());
}

void handleWrite() {
  String text = server.arg("text");
  text = urlDecode(text);

  bool ok = writeNdefUriToTag(text, lastMessage);

  if (!ok) lastMessage = "WRITE FAILED: " + lastMessage;

  lastDump = "";

  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

void handleClear() {
  bool ok = clearTag(lastMessage);

  if (!ok) lastMessage = "CLEAR FAILED: " + lastMessage;

  lastDump = "";

  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

void handleDump() {
  bool ok = dumpTag(lastDump, lastMessage);

  if (!ok) {
    lastMessage = "DUMP FAILED: " + lastMessage;
    lastDump = "";
  }

  server.send(200, "text/html", pageHtml());
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting to WiFi");

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print('.');
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP address: ");
    Serial.println(WiFi.localIP());
    lastMessage = "WiFi connected. IP: " + WiFi.localIP().toString();
  } else {
    Serial.print("WiFi failed, status=");
    Serial.println(WiFi.status());
    lastMessage = "WiFi failed. Check SSID/password and use 2.4 GHz.";
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("ESP32 PN532 NTAG215 NDEF Web Writer/Dumper");

  // ESP32 hardware SPI uses the default pins:
  // SCK=18, MISO=19, MOSI=23. CS=5, reset not connected.
  nfc.InitHardwareSPI(PN532_SS, PN532_RESET);
  nfc.SetDebugLevel(0);
  nfc.begin();

  uint8_t icType = 0;
  uint8_t fwMajor = 0;
  uint8_t fwMinor = 0;
  uint8_t supportFlags = 0;

  if (!nfc.GetFirmwareVersion(&icType, &fwMajor, &fwMinor, &supportFlags)) {
    Serial.println("PN532 not found. Check wiring and SPI/I2C switch.");
    while (1) delay(100);
  }

  Serial.print("PN532 IC: 0x");
  Serial.println(icType, HEX);
  Serial.print("PN532 firmware: ");
  Serial.print(fwMajor);
  Serial.print(".");
  Serial.println(fwMinor);

  if (!nfc.SamConfig()) {
    Serial.println("PN532 SAM configuration failed.");
    while (1) delay(100);
  }

  if (!nfc.SetPassiveActivationRetries()) {
    Serial.println("Warning: could not set passive activation retries.");
  }

  Serial.println("PN532 initialized");

  connectWifi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/write", HTTP_POST, handleWrite);
  server.on("/clear", HTTP_POST, handleClear);
  server.on("/dump", HTTP_GET, handleDump);

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
