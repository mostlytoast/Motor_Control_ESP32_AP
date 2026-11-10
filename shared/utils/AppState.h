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
  void setTemplateTrack(const Track& track);
  Track* getTemplateTrack();
  const Track* getTemplateTrack() const;
  TrackType getSelectedTrackType();
  void setSelectedTrackType(TrackType selectedTrackType_);

  TrackType getPreviousSelectedTrackType();
  // display
  String getStatusLabelText();
  void setStatusLabelText(String statusLabelText_);

  // location
  Uid getDestinationUid();
  void setDestinationUid(Uid destinationUid_);
  Uid getCurrentUid();
  void setCurrentUid(Uid currentUid_);
  void reset(); 
 private:
  void initDefaults();
  // LEDs
  bool led1State;
  bool led2State;

  // Automatic mode
  bool autoState;
  AutoMode autoMode;

  // Scanner
  ScannerMode scannerMode;
  ScannerMode previousScanMode;

  String ScannerStatusLabelText;

  // Audio
  bool ambianceAudioMode;
  bool movementAudioMode;

  float movementAudioPitch;
  float movementAudioSpeed;

  float ambianceAudioPitch;
  float ambianceAudioSpeed;

  // Speed
  volatile int targetSpeed;
  volatile int userSpeed;
  float currentSpeed;

  // Ramp
  unsigned long rampTime;
  unsigned long rampStartTime;
  int rampStartSpeed;
  int lastTargetSpeed;

  // template track
  Track templateTrack_;
  bool hasTemplateTrack_;

  TrackType selectedTrackType;
  TrackType previousSelectedTrackType;
  // display status label
  String statusLabelText;

  // location
  Uid destinationUid;
  Uid currentUid;
};

extern AppState app;