#include "better_motor_controller.h"
// TODO convert everything into classes at least what makes sense
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
  Serial.begin(115200);

  Serial.println("Main setup");

  if (!AudioPlayer_begin()) {
    CLOGE << "AudioPlayer initialization failed";
  }

  setupMotor();

  setMotor(0);

  LedControl_begin();

  nfc.setupNFC();

  setupWifi();

  espnowController.setupESPNow();

  WebServer_begin();

  trackScanner.begin();

  audio.setupAudio();

  Serial.println("ESP-NOW multi-controller receiver ready");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // Process ESP-NOW commands

  espnowController.espnowUpdate();

  // Automatic mode

  autoStop();

  // track scanner
  // TODO find faster way to read maybe do shorter tag data? likely want to do
  // so you dont have to modify library to exced 64 page read limit

  trackScanner.processScans();

  trackScanner.processSerial();

  // Motor ramp

  updateMotorRamp();

  updateLeds();

  delay(5);
}
