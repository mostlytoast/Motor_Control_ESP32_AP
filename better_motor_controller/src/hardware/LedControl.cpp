#include "LedControl.h"

void LedControl_begin() {
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  updateLeds();
}

void updateLeds() {
  digitalWrite(LED1_PIN, app.getLed1State() ? HIGH : LOW);

  digitalWrite(LED2_PIN, app.getLed2State() ? HIGH : LOW);
}

// TODO create a secondary system for the whistle even tho they both act like
// leds, plan on using different port for whistle? also have it ramp up and down