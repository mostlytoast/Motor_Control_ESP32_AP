#include "encoder.h"

#define ENCODER_A 27
#define ENCODER_B 22

#define ENCODER_DETENTS_TO_FULL_SPEED 20

volatile int8_t encoderTransitions = 0;
volatile uint8_t encoderLastState = 0;

void IRAM_ATTR encoderISR() {
  uint8_t a = digitalRead(ENCODER_A);
  uint8_t b = digitalRead(ENCODER_B);

  uint8_t currentState = (a << 1) | b;

  uint8_t transition = (encoderLastState << 2) | currentState;

  switch (transition) {
    // Clockwise
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      encoderTransitions++;
      break;

    // Counter-clockwise
    case 0b0010:
    case 0b1011:
    case 0b1101:
    case 0b0100:
      encoderTransitions--;
      break;

    default:
      // Invalid transition / bounce
      break;
  }

  encoderLastState = currentState;
}

void setupEncoder() {
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);

  uint8_t a = digitalRead(ENCODER_A);
  uint8_t b = digitalRead(ENCODER_B);

  encoderLastState = (a << 1) | b;
  encoderTransitions = 0;

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);

  attachInterrupt(digitalPinToInterrupt(ENCODER_B), encoderISR, CHANGE);
}

void updateEncoder() {
  int transitions;

  // Atomically copy the ISR value
  noInterrupts();
  transitions = encoderTransitions;
  encoderTransitions = 0;
  interrupts();

  if (transitions == 0) {
    return;
  }

  const int transitionsPerDetent = 4;

  // Only process complete detents
  int detentMovement = transitions / transitionsPerDetent;

  // Keep incomplete transitions
  int remainder = transitions % transitionsPerDetent;

  noInterrupts();
  encoderTransitions = remainder;
  interrupts();

  if (detentMovement == 0) {
    return;
  }

  float speedPerDetent = 255.0f / ENCODER_DETENTS_TO_FULL_SPEED;

  int speedChange = round(detentMovement * speedPerDetent);

  if (speedChange == 0) {
    speedChange = detentMovement > 0 ? 1 : -1;
  }

  int motorSpeed = app.getUserSpeed() + speedChange;

  app.setUserSpeed(constrain(motorSpeed, -255, 255));

  // notifyLocalStateChanged();
}