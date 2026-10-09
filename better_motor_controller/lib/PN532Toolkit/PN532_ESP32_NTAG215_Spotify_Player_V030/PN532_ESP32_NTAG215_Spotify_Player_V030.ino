/*
  ============================================================================
  ESP32 Spotify RFID Player V027
  ============================================================================
  Copyright (c) 2026 Bas Kasteel

  Licensed under the MIT License.
  See the LICENSE file in the project root for full license information.
  
  Version history
  ---------------
  V030
   • Managed secret.h 
  V027
   • Replaced card sweep with blue pulse.
   • Blue pulse for every detected tag.
   • Improved LED timing.
   • Slower, more visible error indication.
   • Renamed LED_CARD_SWEEP → LED_CARD_PULSE.

  V023
   • Initial Spotify player release.
   
  Hardware
  --------
  • ESP32 DevKit
  • PN532 NFC reader (SPI)
  • NTAG215 NFC tags
  • WS2812 LED ring (12 LEDs)

  PN532 SPI wiring
  ----------------
    SCK  -> GPIO18
    MISO -> GPIO19
    MOSI -> GPIO23
    SS   -> GPIO5

  WS2812 LED ring
  ----------------
    Data -> GPIO4

  Description
  -----------
  Reads NDEF URI records from NTAG215 NFC tags and controls Spotify playback
  via the Spotify Web API. Supports tracks, albums, playlists, artists and
  special command cards.

  Supported card types
  --------------------
  • Track https://open.spotify.com/track/<id>
  • Album https://open.spotify.com/album/<id>
  • Playlist https://open.spotify.com/playlist/<id>
  • Artist https://open.spotify.com/artist/<id>
  • Shuffle toggle spotify:shuffle

  Card options
  ------------
  • ;volume=<0-100> Set playback volume before playing.
  • ;count=<tracks> Number of tracks in a playlist/album. Used to randomly select a starting track.

  Playback behaviour
  ------------------
  • Track cards play the selected track.
  • Album and playlist cards start at a random track.
  • Presenting the same album/playlist card again skips to the next track.
  • Automatically refreshes expired Spotify access tokens.
  • Automatically transfers playback to the configured Spotify device.

  LED indications
  ---------------
  Rotating Blue     Connecting to Wi-Fi
  Breathing Green   Ready for a card
  Blue Pulse        Card detected and reading
  Solid Green       Command accepted
  Flashing Red      Invalid card or error

  Notes
  -----
  • Standard Spotify cards contain: https://open.spotify.com/<type>/<id>
  • Supports both Spotify URLs and Spotify URIs.

  ============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <FastLED.h>
#include "PN532.h"
#include <esp_heap_caps.h>

#include "secret.h"
#include "SpotifyClient.h"

// ================= PN532 SPI =================
#define PN532_SCK   18
#define PN532_MISO  19
#define PN532_MOSI  23
#define PN532_SS    5

// Uses the merged PN532.h / PN532.cpp files stored in this sketch folder.
PN532 nfc;


// ================= WS2812 LED ring =================
#define LED_PIN         4
#define LED_COUNT       12

// FastLED uses 0...255. 128 limits the whole ring to approximately 50%.
#define LED_BRIGHTNESS  40

CRGB leds[LED_COUNT];

enum LedState {
  LED_WIFI_CONNECTING,
  LED_READY,
  LED_CARD_PULSE,
  LED_SUCCESS,
  LED_ERROR
};

LedState ledState = LED_WIFI_CONNECTING;
unsigned long ledStateStartedMs = 0;
unsigned long lastLedFrameMs = 0;

const unsigned long CARD_PULSE_MS = 150;
const unsigned long SUCCESS_MS    = 800;
const unsigned long ERROR_MS      = 600;

void setLedState(LedState newState) {
  ledState = newState;
  ledStateStartedMs = millis();
  lastLedFrameMs = 0;
}

void updateLedAnimation() {
  const unsigned long now = millis();

  if (now - lastLedFrameMs < 20) return;
  lastLedFrameMs = now;

  switch (ledState) {
    case LED_WIFI_CONNECTING: {
      fill_solid(leds, LED_COUNT, CRGB::Black);

      uint8_t pos = (now / 85) % LED_COUNT;
      leds[pos] = CRGB(0, 0, 255);
      leds[(pos + LED_COUNT - 1) % LED_COUNT] = CRGB(0, 0, 55);

      FastLED.show();
      break;
    }

    case LED_READY: {
      // Gentle breathing green. Global brightness still caps the ring at 50%.
      uint8_t wave = sin8((now / 8) & 0xFF);
      uint8_t level = map(wave, 0, 255, 12, 95);

      fill_solid(leds, LED_COUNT, CRGB(0, level, 0));
      FastLED.show();
      break;
    }

    case LED_CARD_PULSE: {
      unsigned long elapsed = now - ledStateStartedMs;

      fill_solid(leds, LED_COUNT, CRGB::Blue);
      FastLED.show();

      if (elapsed >= CARD_PULSE_MS) {
         setLedState(LED_READY);
      }
      break;
    }

    case LED_SUCCESS: {
      unsigned long elapsed = now - ledStateStartedMs;

      // Start solid green, then fade smoothly into the ready state.
      uint8_t level = 255;

      if (elapsed > 550) {
        level = map(min(elapsed, SUCCESS_MS), 550UL, SUCCESS_MS, 255, 70);
      }

      fill_solid(leds, LED_COUNT, CRGB(0, level, 0));
      FastLED.show();

      if (elapsed >= SUCCESS_MS) {
        setLedState(LED_READY);
      }
      break;
    }

case LED_ERROR: {
  unsigned long elapsed = now - ledStateStartedMs;

  // Two clear red flashes:
  // 250 ms on, 100 ms off, 250 ms on.
  bool on =
    (elapsed < 250) ||
    (elapsed >= 350 && elapsed < 600);

  fill_solid(
    leds,
    LED_COUNT,
    on ? CRGB::Red : CRGB::Black
  );
  FastLED.show();

  if (elapsed >= ERROR_MS) {
    setLedState(LED_READY);
  }
  break;
}
  }
}

void startCardPulse() {
  setLedState(LED_CARD_PULSE);
}

void showLedSuccess() {
  setLedState(LED_SUCCESS);
}

void showLedError() {
  setLedState(LED_ERROR);
}

bool spotifyHttpSucceeded(int code) {
  return code >= 200 && code < 300;
}

// ================= NTAG215 =================
static const uint8_t NTAG215_FIRST_USER_PAGE = 4;
static const uint8_t NTAG215_LAST_NDEF_PAGE  = 129;
static const uint16_t NTAG215_USER_BYTES =
  (NTAG215_LAST_NDEF_PAGE - NTAG215_FIRST_USER_PAGE + 1) * 4;

SpotifyClient spotify = SpotifyClient(clientId, clientSecret, deviceName, refreshToken);

String currentUid = "";
String lastSeenUid = "";
String lastPlayedUri = "";
unsigned long lastPlayMs = 0;

const unsigned long SAME_CARD_REPEAT_MS = 5000;

// ---------- Small helpers ----------
String ndefUriPrefix(uint8_t code) {
  switch (code) {
    case 0x00: return "";
    case 0x01: return "http://www.";
    case 0x02: return "https://www.";
    case 0x03: return "http://";
    case 0x04: return "https://";
    case 0x05: return "tel:";
    case 0x06: return "mailto:";
    case 0x07: return "ftp://anonymous:anonymous@";
    case 0x08: return "ftp://ftp.";
    case 0x09: return "ftps://";
    case 0x0A: return "sftp://";
    case 0x0B: return "smb://";
    case 0x0C: return "nfs://";
    case 0x0D: return "ftp://";
    case 0x0E: return "dav://";
    case 0x0F: return "news:";
    case 0x10: return "telnet://";
    case 0x11: return "imap:";
    case 0x12: return "rtsp://";
    case 0x13: return "urn:";
    case 0x14: return "pop:";
    case 0x15: return "sip:";
    case 0x16: return "sips:";
    case 0x17: return "tftp:";
    case 0x18: return "btspp://";
    case 0x19: return "btl2cap://";
    case 0x1A: return "btgoep://";
    case 0x1B: return "tcpobex://";
    case 0x1C: return "irdaobex://";
    case 0x1D: return "file://";
    case 0x1E: return "urn:epc:id:";
    case 0x1F: return "urn:epc:tag:";
    case 0x20: return "urn:epc:pat:";
    case 0x21: return "urn:epc:raw:";
    case 0x22: return "urn:epc:";
    case 0x23: return "urn:nfc:";
    default:   return "";
  }
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
  Iso14443AInfo info;

  // Use the same ISO14443A selection path as the proven Avatar sketch.
  bool ok = nfc.readIso14443A(info);

  if (!ok) {
    msg = "PN532 communication error";
    return false;
  }

  if (!info.present) {
    msg = "No tag";
    return false;
  }

  currentUid = uidToString(info.uid, info.uidLength);
  msg = "UID=" + currentUid;
  return true;
}

bool readNtagUserMemory(uint8_t *data, uint16_t &dataLen, String &err)
{
  dataLen = 0;

  String msg;
  if (!selectTag(msg)) {
    err = msg;
    return false;
  }

  Serial.println(msg);

  uint16_t requiredBytes = 0;

  for (uint16_t page = NTAG215_FIRST_USER_PAGE;
       page <= NTAG215_LAST_NDEF_PAGE;
       page += 4)
  {
    uint8_t block[16];

    if (!nfc.ntagRead16((byte)page, block)) {
      err = "Card removed during read";
      return false;
    }

    for (uint8_t i = 0;
         i < 16 && dataLen < NTAG215_USER_BYTES;
         i++)
    {
      data[dataLen++] = block[i];
    }

    /*
       Standard NTAG NDEF memory begins with:

       03 LL ...NDEF data... FE

       03 = NDEF TLV
       LL = NDEF length

       For lengths of 255 bytes or more:

       03 FF HH LL
    */
    if (requiredBytes == 0 && dataLen >= 2) {
      uint16_t p = 0;

      // Skip NULL TLVs.
      while (p < dataLen && data[p] == 0x00) {
        p++;
      }

      if (p < dataLen && data[p] == 0x03) {
        if (p + 1 < dataLen) {
          if (data[p + 1] != 0xFF) {
            requiredBytes = p + 2 + data[p + 1];
          }
          else if (p + 3 < dataLen) {
            uint16_t ndefLength =
              ((uint16_t)data[p + 2] << 8) |
              data[p + 3];

            requiredBytes = p + 4 + ndefLength;
          }
        }
      }
    }

    // We now have the complete NDEF TLV.
    if (requiredBytes > 0 && dataLen >= requiredBytes) {
      return true;
    }

    // Stop when an NDEF terminator is encountered.
    if (dataLen > 0 && data[dataLen - 1] == 0xFE) {
      return true;
    }
  }

  return true;
}

String parseNdefUri(const uint8_t *data, uint16_t len) {
  uint16_t i = 0;

  while (i < len) {
    uint8_t tlv = data[i++];

    if (tlv == 0x00) continue;
    if (tlv == 0xFE) break;
    if (tlv != 0x03) continue;

    if (i >= len) return "";

    uint32_t tlvLen = data[i++];

    if (tlvLen == 0xFF) {
      if (i + 1 >= len) return "";
      tlvLen = ((uint16_t)data[i] << 8) | data[i + 1];
      i += 2;
    }

    if (i + tlvLen > len) tlvLen = len - i;

    uint16_t r = i;
    uint16_t end = i + tlvLen;

    if (r >= end) return "";

    uint8_t header = data[r++];
    bool sr = header & 0x10;
    bool il = header & 0x08;
    uint8_t tnf = header & 0x07;

    if (r >= end) return "";
    uint8_t typeLen = data[r++];

    uint32_t payloadLen = 0;

    if (sr) {
      if (r >= end) return "";
      payloadLen = data[r++];
    } else {
      if (r + 3 >= end) return "";
      payloadLen =
        ((uint32_t)data[r] << 24) |
        ((uint32_t)data[r + 1] << 16) |
        ((uint32_t)data[r + 2] << 8) |
        data[r + 3];
      r += 4;
    }

    uint8_t idLen = 0;
    if (il) {
      if (r >= end) return "";
      idLen = data[r++];
    }

    if (r + typeLen + idLen + payloadLen > end) return "";

    uint16_t typePos = r;
    r += typeLen;
    r += idLen;
    uint16_t payloadPos = r;

    if (tnf == 0x01 && typeLen == 1 && data[typePos] == 0x55 && payloadLen >= 1) {
      String uri = ndefUriPrefix(data[payloadPos]);

      for (uint32_t k = 1; k < payloadLen; k++) {
        char c = (char)data[payloadPos + k];
        if (c == 0x00 || c == (char)0xFE) break;
        uri += c;
      }

      return uri;
    }

    String fallback = "";
    for (uint32_t k = 0; k < payloadLen; k++) {
      char c = (char)data[payloadPos + k];
      if (c >= 32 && c <= 126) fallback += c;
    }

    return fallback;
  }

  return "";
}

String stripUrlJunk(String s) {
  int semi = s.indexOf(';');

  String base = s;
  String cmd = "";

  if (semi >= 0) {
    base = s.substring(0, semi);
    cmd  = s.substring(semi);
  }

  int q = base.indexOf('?');
  if (q >= 0) base = base.substring(0, q);

  int hash = base.indexOf('#');
  if (hash >= 0) base = base.substring(0, hash);

  while (base.endsWith("/")) base.remove(base.length() - 1);

  return base + cmd;
}

String spotifyFromNdefUri(String uri) {
  uri.trim();

  if (uri.startsWith("spotify:")) {
    return stripUrlJunk(uri);
  }

  int p = uri.indexOf("open.spotify.com/");
  if (p < 0) {
    if (uri.startsWith("track/") || uri.startsWith("album/") ||
        uri.startsWith("playlist/") || uri.startsWith("artist/") ||
        uri.startsWith("show/") || uri.startsWith("episode/")) {
      uri.replace('/', ':');
      return "spotify:" + stripUrlJunk(uri);
    }

    return "";
  }

  String tail = uri.substring(p + strlen("open.spotify.com/"));
  tail = stripUrlJunk(tail);
  tail.replace('/', ':');

  if (tail.length() == 0) return "";

  return "spotify:" + tail;
}

void dumpUserBytesShort(const uint8_t *data, uint16_t len) {
  Serial.println("NDEF/user memory first bytes:");

  for (uint16_t i = 0; i < len && i < 96; i++) {
    if (i % 16 == 0) {
      Serial.println();
      Serial.print(i);
      Serial.print(": ");
    }

    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }

  Serial.println();
}

bool playSpotifyUri(String spotifyUri) {
  if (spotifyUri.length() == 0) return false;

  if (spotifyUri == "spotify:shuffle") {
    Serial.println("Shuffle toggle card");

    int state = spotify.GetShuffleState();

    if (state < 0) {
      Serial.println("Could not determine current shuffle state.");
      return false;
    }

    bool newState = (state == 0);

    Serial.print("Shuffle was: ");
    Serial.println(state == 1 ? "ON" : "OFF");

    int code = spotify.SetShuffle(newState);

    if (code == 401) {
      Serial.println("Token expired, refreshing...");
      spotify.FetchToken();
      code = spotify.SetShuffle(newState);
    }

    if (spotifyHttpSucceeded(code)) {
      Serial.print("Shuffle is now: ");
      Serial.println(newState ? "ON" : "OFF");
      return true;
    }

    return false;
  }

  Serial.print("Playing Spotify URI: ");
  Serial.println(spotifyUri);

  String mainUri = spotifyUri;
  String commands = "";

  int semi = spotifyUri.indexOf(';');
  if (semi >= 0) {
    mainUri = spotifyUri.substring(0, semi);
    commands = spotifyUri.substring(semi + 1);
    commands.trim();
  }

  Serial.print("Main URI: ");
  Serial.println(mainUri);

  static bool deviceActivated = false;

  if (!deviceActivated) {
    spotify.TransferPlayback();
    delay(200);
    deviceActivated = true;
  }

  int volume = -1;
  int count = -1;

  while (commands.length() > 0) {
    int p = commands.indexOf(';');
    String cmd;

    if (p >= 0) {
      cmd = commands.substring(0, p);
      commands = commands.substring(p + 1);
    } else {
      cmd = commands;
      commands = "";
    }

    cmd.trim();

    if (cmd.startsWith("volume=")) {
      volume = cmd.substring(7).toInt();
    } else if (cmd.startsWith("count=")) {
      count = cmd.substring(6).toInt();
    }
  }

  if (volume >= 0) {
    Serial.print("Setting volume: ");
    Serial.println(volume);
    spotify.SetVolume(volume);
    delay(200);
  }

  int offset = -1;

  if ((mainUri.startsWith("spotify:playlist:") ||
       mainUri.startsWith("spotify:album:")) && count > 0) {
    offset = random(count);
    Serial.print("Random offset: ");
    Serial.println(offset);
  }

  int code = spotify.Play(mainUri, offset);

  if (code == 401) {
    Serial.println("Token expired, refreshing...");
    spotify.FetchToken();

    if (volume >= 0) {
      spotify.SetVolume(volume);
      delay(200);
    }

    code = spotify.Play(mainUri, offset);

    Serial.print("Play retry result: ");
    Serial.println(code);
  }

  return spotifyHttpSucceeded(code);
}

void connectWifi() {
  setLedState(LED_WIFI_CONNECTING);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting to WiFi");

  unsigned long start = millis();
  unsigned long lastDot = 0;

  while (WiFi.status() != WL_CONNECTED && millis() - start < 25000) {
    updateLedAnimation();

    if (millis() - lastDot >= 500) {
      lastDot = millis();
      Serial.print('.');
    }

    delay(5);
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("WiFi failed. Status=");
    Serial.println(WiFi.status());
    showLedError();
  }
}

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.clear(true);
  setLedState(LED_WIFI_CONNECTING);

  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("ESP32 PN532 NTAG215 Spotify player V024 starting");

  randomSeed(esp_random());

  connectWifi();

  // ESP32 VSPI default pins are SCK=18, MISO=19 and MOSI=23.
  // SS is GPIO5. No PN532 reset pin is connected, so use 0xFF.
  nfc.InitHardwareSPI(PN532_SS, 0xFF);
  nfc.SetDebugLevel(0);
  nfc.begin();

  byte icType = 0;
  byte fwMajor = 0;
  byte fwMinor = 0;
  byte supportFlags = 0;

  if (!nfc.GetFirmwareVersion(&icType, &fwMajor, &fwMinor, &supportFlags)) {
    Serial.println("PN532 not found. Check wiring and the SPI/I2C selector.");
    showLedError();
    while (1) { updateLedAnimation(); delay(5); }
  }

  Serial.print("PN532 IC: 0x");
  Serial.println(icType, HEX);
  Serial.print("PN532 firmware: ");
  Serial.print(fwMajor);
  Serial.print('.');
  Serial.println(fwMinor);

  if (!nfc.SetPassiveActivationRetries()) {
    Serial.println("Warning: could not set passive activation retries.");
  }

  if (!nfc.SamConfig()) {
    Serial.println("PN532 SAM configuration failed.");
    showLedError();
    while (1) { updateLedAnimation(); delay(5); }
  }

  Serial.println("PN532 initialized");

  if (WiFi.status() == WL_CONNECTED) {
    spotify.FetchToken();
    spotify.GetDevices();
    spotify.TransferPlayback();
    delay(500);
  }

  Serial.println("Present NTAG215 card with Spotify NDEF URI.");
  setLedState(LED_READY);
}

void loop()
{
  static unsigned long lastScanMs = 0;

  updateLedAnimation();

  // Fast card detection without continuously hammering the PN532.
  if (millis() - lastScanMs < 50) {
    return;
  }

  lastScanMs = millis();

  static uint32_t lastHeap = 0;

if (millis() - lastHeap > 60000) {
    lastHeap = millis();

Serial.print("Free=");
Serial.print(ESP.getFreeHeap());
Serial.print(" Largest=");
Serial.print(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
Serial.print(" Min=");
Serial.println(ESP.getMinFreeHeap());
}

  static uint8_t data[NTAG215_USER_BYTES];
  uint16_t dataLen = 0;
  String err;

  if (!readNtagUserMemory(data, dataLen, err)) {
    return;
  }

  // Any successfully detected/read tag gets an immediate blue acknowledgement.
  startCardPulse();

  while (ledState == LED_CARD_PULSE) {
    updateLedAnimation();
    delay(5);
  }

  String uid = currentUid;
  String ndefUri = parseNdefUri(data, dataLen);

  if (ndefUri.length() == 0) {
    Serial.println("Tag found, but no NDEF URI found.");
    dumpUserBytesShort(data, dataLen);
    showLedError();

    Serial.println("Remove card...");

    while (true) {
      updateLedAnimation();

      String dummy;
      if (!selectTag(dummy)) break;

      delay(5);
    }

    Serial.println("Ready for next card.");
    delay(30);
    return;
  }

  Serial.print("NDEF URI: ");
  Serial.println(ndefUri);

  String spotifyUri = spotifyFromNdefUri(ndefUri);

  if (spotifyUri.length() == 0) {
    Serial.println("NDEF URI is not a Spotify URL/URI.");
    showLedError();

    Serial.println("Remove card...");

    while (true) {
      updateLedAnimation();

      String dummy;
      if (!selectTag(dummy)) break;

      delay(5);
    }

    Serial.println("Ready for next card.");
    delay(30);
    return;
  }

  bool sameCardAndUri =
    (uid == lastSeenUid && spotifyUri == lastPlayedUri);

  bool commandAccepted = false;

  if (sameCardAndUri &&
      (spotifyUri.startsWith("spotify:playlist:") ||
       spotifyUri.startsWith("spotify:album:"))) {

    Serial.println("Same playlist/album card -> next track");

    if (WiFi.status() != WL_CONNECTED) {
      connectWifi();
    }

    if (WiFi.status() == WL_CONNECTED) {
      int code = spotify.Next();

      if (code == 401) {
        Serial.println("Token expired, refreshing...");
        spotify.FetchToken();
        code = spotify.Next();
      }

      Serial.print("Next HTTP: ");
      Serial.println(code);

      // Any HTTP 2xx response is a successful Spotify command.
      commandAccepted = spotifyHttpSucceeded(code);
    }
    else {
      Serial.println("No WiFi, cannot select next track.");
    }
  }
  else {
    lastSeenUid = uid;
    lastPlayedUri = spotifyUri;

    if (WiFi.status() != WL_CONNECTED) {
      connectWifi();
    }

    if (WiFi.status() == WL_CONNECTED) {
      commandAccepted = playSpotifyUri(spotifyUri);
      Serial.println("Returned from playSpotifyUri");
    }
    else {
      Serial.println("No WiFi, cannot play Spotify.");
    }
  }

if (commandAccepted) {
  showLedSuccess();
}
else {
  showLedError();
}

  /*
     Do not repeatedly process a card that is still sitting on the reader.
     LED success/error animations continue while waiting for removal.
  */
  Serial.println("Waiting for card removal...");

  while (true) {
    updateLedAnimation();

    String dummy;
    if (!selectTag(dummy)) break;

    delay(5);
  }

  Serial.println("Card removed. Ready for next card.");
  delay(30);
}
