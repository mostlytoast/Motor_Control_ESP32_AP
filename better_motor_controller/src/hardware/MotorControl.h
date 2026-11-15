#pragma once

#include <Arduino.h>

#include "../../../shared/utils/AppState.hpp"
#include "../../../shared/utils/Math.hpp"
#include "../Config.h"

extern int sensorValue;
void setupMotor();
void setMotor(int speed);
void updateMotorRamp();
void setTargetSpeed(int speed);
int getTargetSpeed();

void setUserSpeed(int speed);
int getUserSpeed();

void setRampTime(int speed);
int getRampTime();

// MOVE to audio section
