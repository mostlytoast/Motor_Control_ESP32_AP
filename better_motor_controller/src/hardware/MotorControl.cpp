
#include "MotorControl.h"
// TODO remove hall sensor implementation
// Global variable definitions

void setupMotor() {
  ledcAttachChannel(IN1, freq, resolution, ledChannel);

  ledcAttachChannel(IN2, freq, resolution, ledChannel);

  // todo maybe move hall to auto.h ?
  // pinMode(HALL_PIN, INPUT);

  pinMode(ENA, OUTPUT);

  ledcWrite(ENA, 0);
}

void setMotor(int speed) {
  speed = constrain(speed, -255, 255);

  // ===================================================
  // STOP
  // ===================================================

  if (speed == 0) {
    ledcWrite(IN1, 0);

    ledcWrite(IN2, 0);

    digitalWrite(ENA, LOW);
    app.setMovementAudioMode(false);
    return;
  }
  app.setMovementAudioMode(true);

  // ===================================================
  // Get magnitude
  // ===================================================

  int pwm = abs(speed);

  // Convert commanded speed
  // 1-255 into MIN_PWM-255

  pwm = map(pwm, 1, 255, MIN_PWM, MAX_PWM);

  pwm = constrain(pwm, MIN_PWM, MAX_PWM);

  // ===================================================
  // FORWARD
  // ===================================================

  if (speed > 0) {
    ledcWrite(IN1, pwm);

    ledcWrite(IN2, 0);
    digitalWrite(ENA, HIGH);
  }

  // ===================================================
  // REVERSE
  // ===================================================

  else {
    ledcWrite(IN1, 0);

    ledcWrite(IN2, pwm);
    digitalWrite(ENA, HIGH);
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

// ===================================================
// Command interface for ESP-NOW and WebAPI
// ===================================================

void changeSpeedCommand(int speed) {
  app.setTargetSpeed(speed);

  app.setUserSpeed(app.getTargetSpeed());

  Serial.print("Target speed: ");
  Serial.println(app.getTargetSpeed());
}

void changeRampCommand(unsigned long ramp) {
  ramp = clampInt(ramp, 0, 10000);

  app.setRampTime(ramp);

  Serial.print("Ramp time: ");
  Serial.print(app.getRampTime() / 1000.0, 1);
  Serial.println(" seconds");
}

// ===================================================
// Emergency motor stop
// ===================================================

void motorStopCommand() {
  app.setTargetSpeed(0);

  app.setUserSpeed(0);

  app.setAutoState(false);

  Serial.println("EMERGENCY STOP");
}
