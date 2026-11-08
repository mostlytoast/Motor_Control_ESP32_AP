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
  display.setupDisplay();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  updateEncoder();

  display.refresh();


}
