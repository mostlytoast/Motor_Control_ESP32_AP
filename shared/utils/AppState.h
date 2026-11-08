#pragma once

#include <Arduino.h>

#include "../track/map.h"

// TODO all values should have a default state defined here maybe?? and not be
// immediately overwrite by other code

enum AutoMode { AUTO_IDLE, AUTO_MOVING, AUTO_WAITING, AUTO_DEPARTING };

enum ScannerMode {
  REGISTER_TRACKS,
  WRITING_TRACKS,
  SCANNING_TRACKS,
  PATH_FINDING,
  NORMAL
};

class AppState {
 public:
  AppState();

  // LEDs

  bool getLed1State() const;
  void setLed1State(bool state);

  bool getLed2State() const;
  void setLed2State(bool state);

  // Automatic mode

  bool getAutoState() const;
  void setAutoState(bool state);

  AutoMode getAutoMode() const;
  void setAutoMode(AutoMode mode);

  // Scanner mode

  ScannerMode getScannerMode();
  void setScannerMode(ScannerMode mode);

  ScannerMode getPreviousScannerMode();

  void setScannerStatusLabelText(String statusLabelText_);
  String getScannerStatusLabelText();

  // Audio mode

  bool getAmbianceAudioMode() const;
  void setAmbianceAudioMode(bool state);

  bool getMovementAudioMode() const;
  void setMovementAudioMode(bool state);

  bool getAudioState() const;
  void setAudioState(bool state);

  void setMovementAudioPitch(float pitch);
  void setMovementAudioSpeed(float speed);
  void setAmbianceAudioPitch(float pitch);
  void setAmbianceAudioSpeed(float speed);

  float getMovementAudioPitch();
  float getMovementAudioSpeed();
  float getAmbianceAudioPitch();
  float getAmbianceAudioSpeed();
  // Speed

  int getTargetSpeed() const;
  void setTargetSpeed(int speed);

  int getUserSpeed() const;
  void setUserSpeed(int speed);

  float getCurrentSpeed() const;
  void setCurrentSpeed(float speed);

  int getLastTargetSpeed() const;
  void setLastTargetSpeed(int speed);

  // Ramp

  unsigned long getRampTime() const;
  void setRampTime(unsigned long time);

  unsigned long getRampStartTime() const;
  void setRampStartTime(unsigned long time);

  int getRampStartSpeed() const;
  void setRampStartSpeed(int speed);

  // template track
  Track* getTemplateTrack();
  void setTemplateTrack(Track* templateTrack_);

  // display
  String getStatusLabelText();
  void setStatusLabelText(String statusLabelText_);

  // location
  Uid getDestinationUid();
  void setDestinationUid(Uid destinationUid_);
  Uid getCurrentUid();
  void setCurrentUid(Uid currentUid_);

 private:
  // LEDs
  bool led1State = false;
  bool led2State = false;

  // Automatic mode
  bool autoState = false;
  AutoMode autoMode = AUTO_IDLE;

  // Scanner
  ScannerMode scannerMode = REGISTER_TRACKS;
  ScannerMode previousScanMode = REGISTER_TRACKS;

  String ScannerStatusLabelText;

  // Audio
  bool ambianceAudioMode = false;
  bool movementAudioMode = false;

  float movementAudioPitch = 0;
  float movementAudioSpeed = 0;

  float ambianceAudioPitch = 0;
  float ambianceAudioSpeed = 0;

  // Speed
  volatile int targetSpeed = 0;
  volatile int userSpeed = 0;
  float currentSpeed = 0.0;

  // Ramp
  unsigned long rampTime = 0;
  unsigned long rampStartTime = 0;
  int rampStartSpeed = 0;
  int lastTargetSpeed = 0;

  // template track
  Track* templateTrack = nullptr;

  // display status label
  String statusLabelText;

  // location
  Uid destinationUid;
  Uid currentUid;
};

extern AppState app;