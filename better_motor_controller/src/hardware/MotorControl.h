#pragma once
#include <Arduino.h>

#include <cmath>

#include "../../shared/utils/AppState.h"
#include "../Config.h"
#include "AudioPlayer.h"
extern int movementSound;
extern int ambianceSound;

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

void setLastSensorDetect(int time);
int getSpeedBeforeDetect();

void changeSpeedCommand(int speed);

void changeRampCommand(unsigned long ramp);

bool setAutoStateCommand(bool state);

int toggleAudioAmbianceStateCommand();
int setAudioAmbianceStateCommand(bool state);

int setAudioMovementStateCommand(bool state);

void motorStopCommand();
bool getAutoState();

bool getAmbianceAudioMode();
