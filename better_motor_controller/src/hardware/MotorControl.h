#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "../../../shared/utils/AppState.hpp"
#include "../../../shared/utils/Math.hpp"
#include "../Config.h"

class MotorController {
 public:
  struct RampRatePoint {
    float speedFraction;   // 0.0 - 1.0 of maximum speed
    float rateMultiplier;  // Multiplier applied to the base ramp rate
  };

  MotorController();

  bool begin(int in1, int in2, int ena, uint32_t frequency, uint8_t resolution,
             uint8_t in1Channel = 0, uint8_t in2Channel = 1);

  // Speed configuration
  void setSpeedRange(int minimumSpeed, int maximumSpeed, int maximumPwm);

  // Ramp configuration
  void setRampTimeMs(uint32_t rampTimeMs);

  bool setRampRateCurve(const RampRatePoint* points, size_t count);

  void setMaxUpdateIntervalMs(uint32_t interval);

  // Kick-start configuration
  void setKickStart(int maximumSpeed, int kickPwm, uint32_t durationMs);

  // Motor control
  void update(int targetSpeed, int minimumPwm);
  void setOutputSpeed(int speed, int minimumPwm);

  // Getters
  float getCurrentSpeed() const;
  int getTargetSpeed() const;
  bool isKickStartActive() const;

 private:
  static const size_t MAX_RAMP_POINTS = 8;

  // Pins
  int in1Pin_;
  int in2Pin_;
  int enaPin_;

  // PWM configuration
  uint32_t frequency_;
  uint8_t resolution_;

  // Speed configuration
  int minimumSpeed_;
  int maximumSpeed_;
  int maximumPwm_;

  // Speed state
  float currentSpeed_;
  int targetSpeed_;

  // Ramp configuration
  uint32_t rampTimeMs_;
  uint32_t maxUpdateIntervalMs_;
  uint32_t lastUpdateTime_;
  bool initialized_;

  float baseRampRate_;

  RampRatePoint rampPoints_[MAX_RAMP_POINTS];
  size_t rampPointCount_;

  // Kick-start configuration
  int kickStartThreshold_;
  int kickStartPwm_;
  uint32_t kickStartDuration_;

  bool kickStartActive_;
  uint32_t kickStartTime_;
  int kickStartDirection_;
  int lastDirection_;

  // Internal functions
  float getRampMultiplier(float speed) const;

  void updateKickStart(uint32_t now);

  void applyMotorOutput(float speed, int minimumPwm);

  void stopMotor();

  int calculatePwm(int speed, int minimumPwm) const;

  int getDirection(float speed) const;

  float getAbsolute(float value) const;
};

// Global instance
extern MotorController motorController;

// Compatibility functions
void setupMotor();
void setMotor(int speed);
void updateMotorRamp();