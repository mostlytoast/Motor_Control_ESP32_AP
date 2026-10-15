#pragma once

#include <Arduino.h>

// #include "../SharedConfig.h"
// #include "../communication/packets.h"
#include "../track/map.h"

enum AutoMode { AUTO_IDLE, AUTO_MOVING, AUTO_WAITING, AUTO_DEPARTING };

struct PairedController {
  bool valid = false;
  uint8_t mac[6] = {};
  char name[24] = {};
  unsigned long lastSeen = 0;
};

enum ScannerMode { REGISTER_TRACKS, WRITING_TRACKS, SCANNING_TRACKS };

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

 private:
  // LEDs
  bool led1State = false;
  bool led2State = false;

  // Automatic mode
  bool autoState = false;
  AutoMode autoMode = AUTO_IDLE;

  // Scanner
  ScannerMode scannerMode = SCANNING_TRACKS;
  ScannerMode previousScanMode = SCANNING_TRACKS;

  // Audio
  bool ambianceAudioMode = false;
  bool movementAudioMode = false;

  float  movementAudioPitch = 0;
  float  movementAudioSpeed = 0;

  float  ambianceAudioPitch = 0;
  float  ambianceAudioSpeed = 0;

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
};

extern AppState app;