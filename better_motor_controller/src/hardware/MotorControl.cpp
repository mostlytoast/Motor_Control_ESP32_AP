#include "MotorControl.h"

#include <math.h>


#define IN1_CHANNEL 0
#define IN2_CHANNEL 1

MotorController motorController;

// ============================================================
// Constructor
// ============================================================

MotorController::MotorController()
    : in1Pin_(-1),
      in2Pin_(-1),
      enaPin_(-1),

      frequency_(0),
      resolution_(0),

      minimumSpeed_(1),
      maximumSpeed_(255),
      maximumPwm_(255),

      currentSpeed_(0),
      targetSpeed_(0),

      rampTimeMs_(1000),
      maxUpdateIntervalMs_(100),

      lastUpdateTime_(0),
      initialized_(false),

      baseRampRate_(255.0f),

      rampPointCount_(2),

      kickStartThreshold_(0),
      kickStartPwm_(0),
      kickStartDuration_(0),

      kickStartActive_(false),
      kickStartTime_(0),
      kickStartDirection_(0),
      lastDirection_(0) {
  // Default ramp graph: constant rate at every speed.
  rampPoints_[0] = {0.0f, 1.0f};
  rampPoints_[1] = {1.0f, 1.0f};
}

// ============================================================
// Initialization
// ============================================================

bool MotorController::begin(int in1, int in2, int ena, uint32_t frequency,
                            uint8_t resolution, uint8_t in1Channel,
                            uint8_t in2Channel) {
  in1Pin_ = in1;
  in2Pin_ = in2;
  enaPin_ = ena;

  frequency_ = frequency;
  resolution_ = resolution;

  bool in1Attached =
      ledcAttachChannel(in1Pin_, frequency_, resolution_, in1Channel);

  bool in2Attached =
      ledcAttachChannel(in2Pin_, frequency_, resolution_, in2Channel);

  pinMode(enaPin_, OUTPUT);

  stopMotor();

  lastUpdateTime_ = millis();
  initialized_ = true;

  return in1Attached && in2Attached;
}

// ============================================================
// Speed Configuration
// ============================================================

void MotorController::setSpeedRange(int minimumSpeed, int maximumSpeed,
                                    int maximumPwm) {
  minimumSpeed_ = clampInt(minimumSpeed, 0, maximumSpeed);
  maximumSpeed_ = maximumSpeed;
  maximumPwm_ = maximumPwm;

  if (rampTimeMs_ > 0) {
    baseRampRate_ = (float)maximumSpeed_ / ((float)rampTimeMs_ / 1000.0f);
  }
}

// ============================================================
// Ramp Configuration
// ============================================================

void MotorController::setRampTimeMs(uint32_t rampTimeMs) {
  rampTimeMs_ = rampTimeMs;

  if (rampTimeMs_ == 0) {
    baseRampRate_ = 0;
    return;
  }

  baseRampRate_ = (float)maximumSpeed_ / ((float)rampTimeMs_ / 1000.0f);
}

// Configure how quickly the motor reaches different speeds.
//
// X = Fraction of maximum speed.
// Y = Multiplier applied to the base ramp rate.
//
// Both X and Y must be positive.
// X values must increase from 0 to 1.

bool MotorController::setRampRateCurve(const RampRatePoint* points,
                                       size_t count) {
  if (points == nullptr || count < 2 || count > MAX_RAMP_POINTS) {
    return false;
  }

  if (fabsf(points[0].speedFraction) > 0.001f ||
      fabsf(points[count - 1].speedFraction - 1.0f) > 0.001f) {
    return false;
  }

  for (size_t i = 0; i < count; i++) {
    if (points[i].speedFraction < 0.0f || points[i].speedFraction > 1.0f ||
        points[i].rateMultiplier <= 0.0f) {
      return false;
    }

    if (i > 0 && points[i].speedFraction <= points[i - 1].speedFraction) {
      return false;
    }
  }

  for (size_t i = 0; i < count; i++) {
    rampPoints_[i] = points[i];
  }

  rampPointCount_ = count;

  return true;
}

void MotorController::setMaxUpdateIntervalMs(uint32_t interval) {
  maxUpdateIntervalMs_ = interval;
}

// Interpolates between points on the ramp graph.

float MotorController::getRampMultiplier(float speed) const {
  float fraction = getAbsolute(speed) / (float)maximumSpeed_;

  fraction = constrain(fraction, 0.0f, 1.0f);

  for (size_t i = 1; i < rampPointCount_; i++) {
    const RampRatePoint& previous = rampPoints_[i - 1];
    const RampRatePoint& next = rampPoints_[i];

    if (fraction <= next.speedFraction) {
      float segment = (fraction - previous.speedFraction) /
                      (next.speedFraction - previous.speedFraction);

      return previous.rateMultiplier +
             (next.rateMultiplier - previous.rateMultiplier) * segment;
    }
  }

  return rampPoints_[rampPointCount_ - 1].rateMultiplier;
}

// ============================================================
// Kick Start Configuration
// ============================================================

void MotorController::setKickStart(int maximumSpeed, int kickPwm,
                                   uint32_t durationMs) {
  kickStartThreshold_ = clampInt(maximumSpeed, 0, maximumSpeed_);

  kickStartPwm_ = clampInt(kickPwm, 0, maximumPwm_);

  kickStartDuration_ = durationMs;

  if (durationMs == 0 || kickPwm == 0) {
    kickStartActive_ = false;
  }
}

// ============================================================
// Main Update
// ============================================================

void MotorController::update(int targetSpeed, int minimumPwm) {
  uint32_t now = millis();

  // Always read the latest target.
  targetSpeed_ = clampInt(targetSpeed, -maximumSpeed_, maximumSpeed_);

  // Initialize timing.
  if (!initialized_) {
    lastUpdateTime_ = now;
    initialized_ = true;
  }

  uint32_t elapsed = now - lastUpdateTime_;
  lastUpdateTime_ = now;

  // Prevent an unusually long loop delay from causing
  // a large sudden change in motor speed.
  if (maxUpdateIntervalMs_ > 0 && elapsed > maxUpdateIntervalMs_) {
    elapsed = maxUpdateIntervalMs_;
  }

  float deltaTime = (float)elapsed / 1000.0f;

  // ========================================================
  // Continuously recalculate current speed
  // ========================================================

  float difference = (float)targetSpeed_ - currentSpeed_;

  if (rampTimeMs_ == 0) {
    // Instant response when ramp time is disabled.
    currentSpeed_ = (float)targetSpeed_;
  } else if (fabsf(difference) > 0.001f && deltaTime > 0.0f) {
    // Use whichever speed magnitude is greater.
    // This also gives smooth deceleration.
    float profileSpeed =
        fmaxf(getAbsolute(currentSpeed_), getAbsolute((float)targetSpeed_));

    // Calculate the rate using the graph.
    float multiplier = getRampMultiplier(profileSpeed);

    float rampRate = baseRampRate_ * multiplier;

    // Maximum speed change this update.
    float speedStep = rampRate * deltaTime;

    // Move towards the latest target.
    if (fabsf(difference) <= speedStep) {
      currentSpeed_ = (float)targetSpeed_;
    } else if (difference > 0) {
      currentSpeed_ += speedStep;
    } else {
      currentSpeed_ -= speedStep;
    }
  }

  // ========================================================
  // Kick-start
  // ========================================================

  updateKickStart(now);

  // ========================================================
  // Apply motor output
  // ========================================================

  applyMotorOutput(currentSpeed_, minimumPwm);
}

// ============================================================
// Kick Start Update
// ============================================================

void MotorController::updateKickStart(uint32_t now) {
  int currentDirection = getDirection(currentSpeed_);
  int targetDirection = getDirection((float)targetSpeed_);

  // Motor is stopped.
  if (currentDirection == 0) {
    kickStartActive_ = false;
    lastDirection_ = 0;
    return;
  }

  // Detect starting from rest or changing direction.
  if (currentDirection != lastDirection_) {
    lastDirection_ = currentDirection;

    // Only kick-start when the requested speed is low.
    if (targetDirection == currentDirection &&
        getAbsolute((float)targetSpeed_) <= kickStartThreshold_ &&
        kickStartDuration_ > 0 && kickStartPwm_ > 0) {
      kickStartActive_ = true;
      kickStartTime_ = now;
      kickStartDirection_ = currentDirection;
    } else {
      kickStartActive_ = false;
    }
  }

  // Check whether the kick-start should end.
  if (kickStartActive_) {
    bool expired = (uint32_t)(now - kickStartTime_) >= kickStartDuration_;

    bool stoppedOrReversing = targetDirection != kickStartDirection_;

    bool speedTooHigh = getAbsolute((float)targetSpeed_) > kickStartThreshold_;

    if (expired || stoppedOrReversing || speedTooHigh) {
      kickStartActive_ = false;
    }
  }
}

// ============================================================
// Motor Output
// ============================================================

void MotorController::applyMotorOutput(float speed, int minimumPwm) {
  int logicalSpeed = (int)roundf(speed);

  if (logicalSpeed == 0) {
    stopMotor();
    return;
  }

  int pwm = calculatePwm(abs(logicalSpeed), minimumPwm);

  // Temporarily boost PWM while kick-start is active.
  if (kickStartActive_ && getDirection(speed) == kickStartDirection_) {
    pwm = max(pwm, kickStartPwm_);
  }

  pwm = clampInt(pwm, 0, maximumPwm_);

  digitalWrite(enaPin_, HIGH);

  if (logicalSpeed > 0) {
    ledcWrite(in1Pin_, pwm);
    ledcWrite(in2Pin_, 0);
  } else {
    ledcWrite(in1Pin_, 0);
    ledcWrite(in2Pin_, pwm);
  }
}

// Immediate motor output, without ramping.

void MotorController::setOutputSpeed(int speed, int minimumPwm) {
  targetSpeed_ = clampInt(speed, -maximumSpeed_, maximumSpeed_);

  currentSpeed_ = (float)targetSpeed_;

  kickStartActive_ = false;
  lastDirection_ = getDirection(currentSpeed_);

  applyMotorOutput(currentSpeed_, minimumPwm);
}

// ============================================================
// PWM Calculation
// ============================================================

int MotorController::calculatePwm(int speed, int minimumPwm) const {
  minimumPwm = clampInt(minimumPwm, 0, maximumPwm_);

  speed = clampInt(speed, minimumSpeed_, maximumSpeed_);

  if (maximumSpeed_ == minimumSpeed_) {
    return maximumPwm_;
  }

  float progress =
      (float)(speed - minimumSpeed_) / (float)(maximumSpeed_ - minimumSpeed_);

  int pwm = (int)roundf(minimumPwm + progress * (maximumPwm_ - minimumPwm));

  return clampInt(pwm, minimumPwm, maximumPwm_);
}

// ============================================================
// Stop Motor
// ============================================================

void MotorController::stopMotor() {
  if (in1Pin_ >= 0) {
    ledcWrite(in1Pin_, 0);
  }

  if (in2Pin_ >= 0) {
    ledcWrite(in2Pin_, 0);
  }

  if (enaPin_ >= 0) {
    digitalWrite(enaPin_, LOW);
  }
}

// ============================================================
// Helpers
// ============================================================

int MotorController::getDirection(float speed) const {
  if (speed > 0.001f) return 1;
  if (speed < -0.001f) return -1;

  return 0;
}

float MotorController::getAbsolute(float value) const { return fabsf(value); }

// ============================================================
// Getters
// ============================================================

float MotorController::getCurrentSpeed() const { return currentSpeed_; }

int MotorController::getTargetSpeed() const { return targetSpeed_; }

bool MotorController::isKickStartActive() const { return kickStartActive_; }

// ============================================================
// Compatibility Functions
// ============================================================

void setupMotor() {
  motorController.setSpeedRange(MIN_PWM, MAX_PWM, MAX_PWM);

  bool success = motorController.begin(IN1, IN2, ENA, freq, resolution,
                                       IN1_CHANNEL, IN2_CHANNEL);

  if (!success) {
    Serial.println("[Motor] Failed to attach LEDC channels");
  }

  // Configure the default ramp duration.
  motorController.setRampTimeMs(app.getRampTime());

  // Configure the custom ramp graph.
  const MotorController::RampRatePoint rampCurve[] = {
      {0.0f, 2.5f}, {0.2f, 2.0f}, {0.5f, 1.0f}, {1.0f, 0.4f}};

  motorController.setRampRateCurve(rampCurve,
                                   sizeof(rampCurve) / sizeof(rampCurve[0]));

  // Example kick-start configuration.
  // Adjust these values for your motor.
  motorController.setKickStart(25,  // Only kick-start below this speed
                               (MAX_PWM * 70) / 100,  // Initial kick PWM
                               100  // Kick duration in milliseconds
  );

  Serial.println("[Motor] Initialized");
}

// Immediate motor control, retained for compatibility.
void setMotor(int speed) {
  motorController.setOutputSpeed(speed, app.getMinPwm());

  app.setCurrentSpeed((float)speed);
  app.setMovementAudioState(speed != 0);
}

void updateMotorRamp() {
  motorController.setRampTimeMs(app.getRampTime());

  // Recalculate using the latest target every update.
  motorController.update(app.getTargetSpeed(), app.getMinPwm());

  // Keep the rest of the application synchronized.
  app.setCurrentSpeed(motorController.getCurrentSpeed());

  app.setMovementAudioState(motorController.getCurrentSpeed() != 0.0f);
}