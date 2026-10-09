#include "transmitter.h"
// TODO add usefull debug print lines througout code

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  delay(500);

  Serial.println();

  Serial.println("ESP32 CYD Multi-Receiver Motor Controller");
  setupDisplay();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  // ---------------------------------------------------
  // Encoder
  // ---------------------------------------------------

  updateEncoder();

  // if (app.getServerStateChanged()) {
  //  UIRefresh();
  // }

  UIRefresh();
}
