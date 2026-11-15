
#include "MotorControl.h"
// TODO remove hall sensor implementation
// Global variable definitions
#define IN1_CHANNEL 0
#define IN2_CHANNEL 1
void setupMotor() {
  ledcAttachChannel(IN1, freq, resolution, IN1_CHANNEL);
  ledcAttachChannel(IN2, freq, resolution, IN2_CHANNEL);

  pinMode(ENA, OUTPUT);
  digitalWrite(ENA, LOW);
}

void setMotor(int speed) {
    Serial.print("[Motor] Speed: ");
    Serial.println(speed);

    speed = clampInt(speed, -255, 255);

    if (speed == 0) {
        ledcWrite(IN1, 0);
        ledcWrite(IN2, 0);

        digitalWrite(ENA, LOW);

        app.setMovementAudioMode(false);
        return;
    }

    app.setMovementAudioMode(true);

    int pwm = abs(speed);

    pwm = map(pwm, 1, 255, MIN_PWM, MAX_PWM);
    pwm = clampInt(pwm, MIN_PWM, MAX_PWM);

    digitalWrite(ENA, HIGH);

    if (speed > 0) {
        ledcWrite(IN1, pwm);
        ledcWrite(IN2, 0);
    } else {
        ledcWrite(IN1, 0);
        ledcWrite(IN2, pwm);
    }
}
// Motor ramping

void updateMotorRamp() {
  unsigned long now = millis();

  // ===================================================
  // Target changed
  // ===================================================

  if (app.getTargetSpeed() != app.getLastTargetSpeed()) {
    // Remember starting speed
    app.setRampStartSpeed((int)app.getCurrentSpeed());

    // Start new ramp
    app.setRampStartTime(now);

    // Remember new target
    app.setLastTargetSpeed(app.getTargetSpeed());
  }

  // ===================================================
  // Already at target
  // ===================================================

  if ((int)app.getCurrentSpeed() == app.getTargetSpeed()) {
    return;
  }

  // ===================================================
  // Instant response if ramp = 0
  // ===================================================

  if (app.getRampTime() == 0) {
    app.setCurrentSpeed(app.getTargetSpeed());
    setMotor((int)app.getCurrentSpeed());

    return;
  }

  // ===================================================
  // Calculate progress
  // ===================================================

  unsigned long elapsed = now - app.getRampStartTime();

  float progress = (float)elapsed / (float)app.getRampTime();

  // Limit progress

  if (progress < 0.0) {
    progress = 0.0;
  }

  if (progress > 1.0) {
    progress = 1.0;
  }

  // ===================================================
  // Smoothstep acceleration curve
  // ===================================================

  float curve = progress * progress * (3.0 - 2.0 * progress);

  // ===================================================
  // Calculate current speed
  // ===================================================

  float currentSpeed =
      app.getRampStartSpeed() +
      ((app.getTargetSpeed() - app.getRampStartSpeed()) * curve);

  app.setCurrentSpeed(currentSpeed);

  // ===================================================
  // Snap to target
  // ===================================================

  if (progress >= 1.0) {
    app.setCurrentSpeed(app.getTargetSpeed());
  }

  // ===================================================
  // Apply motor speed
  // ===================================================

  setMotor((int)app.getCurrentSpeed());
}
