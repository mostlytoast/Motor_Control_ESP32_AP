#pragma once

#include <Arduino.h>

#include "../../../shared/utils/AppState.h"
#include "../Config.h"

extern int sensorValue;
void setupMotor();
void setMotor(int speed);
void autoStop();
void updateMotorRamp();
void setTargetSpeed(int speed);
int getTargetSpeed();

void setUserSpeed(int speed);
int getUserSpeed();

void setRampTime(int speed);
int getRampTime();

// void setLastSensorDetect(int time);
// int getSpeedBeforeDetect();

void changeSpeedCommand(int speed);

void changeRampCommand(unsigned long ramp);

bool setAutoStateCommand(bool state);

void motorStopCommand();
bool getAutoState();

// MOVE to audio section
