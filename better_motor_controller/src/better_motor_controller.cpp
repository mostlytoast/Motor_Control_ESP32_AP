#include "better_motor_controller.h"

// ==============================================
// GLOBAL VARIABLE DEFINITIONS
// ==============================================
char serialBuffer[SERIAL_BUFFER_SIZE];
size_t serialBufferLength = 0;

NFC nfc;
TrackMap trackMap;
TrackScanner trackScanner(trackMap);

class WebSerial : public Print {
 public:
  HardwareSerial& serial;

  WebSerial(HardwareSerial& s) : serial(s) {}

  void begin(unsigned long baud) { serial.begin(baud); }

  size_t write(uint8_t c) override {
    serial.write(c);

    if (serialBufferLength >= SERIAL_BUFFER_SIZE - 1) {
      char* newline = strchr(serialBuffer, '\n');

      if (newline != nullptr) {
        size_t removeCount = (newline - serialBuffer) + 1;

        memmove(serialBuffer, serialBuffer + removeCount,
                serialBufferLength - removeCount);

        serialBufferLength -= removeCount;

      }

      else {
        size_t removeCount = SERIAL_BUFFER_SIZE / 2;

        memmove(serialBuffer, serialBuffer + removeCount,
                serialBufferLength - removeCount);

        serialBufferLength -= removeCount;
      }
    }

    serialBuffer[serialBufferLength++] = c;

    serialBuffer[serialBufferLength] = '\0';

    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    for (size_t i = 0; i < size; i++) {
      write(buffer[i]);
    }

    return size;
  }
};

WebSerial WebSerialPort(Serial);

#define Serial WebSerialPort

// =====================================================
// SETUP
// =====================================================

void setup() {
  // float diameter = 36.0;
  // float length = (diameter * M_PI) / 8;
  // Uid emptyUid;

  // trackMap.createTrack(CURVED, length, diameter / 2, 0, 0, 0, false,
  // emptyUid,
  //                      emptyUid, emptyUid, emptyUid);
  // trackMap.calculateMap();
  // app.setScannerMode(REGISTER_TRACKS);
  Serial.begin(115200);

  CLOGI << "Main setup";

  loadControllers();

  app.setPairingModeStarted(millis());

  if (!AudioPlayer_begin()) {
    CLOGE << "AudioPlayer initialization failed";
  }

  setupMotor();

  setMotor(0);

  LedControl_begin();

  nfc.setupNFC();

  // ===================================================
  // WIFI AP + STA
  // ===================================================

  WiFi.mode(WIFI_AP_STA);

  WiFi.softAP(AP_SSID, AP_PASSWORD, WIFI_CHANNEL);

  delay(1000);

  // Keep ESP-NOW on the AP channel.

  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t primaryChannel;
  wifi_second_chan_t secondChannel;

  esp_wifi_get_channel(&primaryChannel, &secondChannel);

  Serial.print("Actual ESP-NOW channel: ");
  Serial.println(primaryChannel);

  Serial.println();

  Serial.println("Access Point started!");

  Serial.print("Network: ");

  Serial.println(AP_SSID);

  Serial.print("IP Address: ");

  Serial.println(WiFi.softAPIP());

  Serial.print("WiFi Channel: ");

  Serial.println(WIFI_CHANNEL);

  Serial.print("Receiver MAC: ");

  Serial.println(WiFi.macAddress());
  Serial.println("=== STARTING WIFI ===");

  WiFi.mode(WIFI_AP_STA);

  Serial.println("WiFi mode set");

  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, WIFI_CHANNEL);

  Serial.print("softAP result: ");
  Serial.println(apStarted ? "SUCCESS" : "FAILED");

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);

  Serial.println("=== WIFI COMPLETE ===");
  setupESPNow();

  WebServer_begin();

  Serial.println("Webserver started!");

  Serial.println("ESP-NOW multi-controller receiver ready");
  trackScanner.begin();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // ---------------------------------------------------
  // Pairing timeout
  // ---------------------------------------------------

  // ESPNow_updatePairingMode();

  // ---------------------------------------------------
  // Process ESP-NOW commands
  // ---------------------------------------------------

  espnowUpdate();

  // ---------------------------------------------------
  // Failsafe
  // ---------------------------------------------------

  // ESPNow_checkFailsafe();

  // ---------------------------------------------------
  // Automatic mode
  // ---------------------------------------------------

  autoStop();

  // TODO find faster way to read maybe do shorter tag data? likely want to do
  // so you dont have to modify library to exced 64 page read limit

  trackScanner.processScans();
  trackScanner.processSerial();

  // ---------------------------------------------------
  // Motor ramp
  // ---------------------------------------------------

  updateMotorRamp();
  updateLeds();
  // notifyLocalStateChanged();
  delay(5);
}
