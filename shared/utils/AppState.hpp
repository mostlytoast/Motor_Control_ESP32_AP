#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <Arduino.h>

#include <vector>

#include "../track/Point.hpp"
#include "../track/Tag.hpp"
#include "../track/Tracks.hpp"
#include "uidLogic.h"
enum AutoMode { AUTO_IDLE, AUTO_MOVING, AUTO_WAITING, AUTO_DEPARTING };

enum ScannerMode {
  NORMAL,
  REGISTER_TRACKS,
  WRITING_TRACKS,
  SCANNING_TRACKS,
  PATH_FINDING
};
// AppState app;

class AppState {
 public:
  // -------------------------
  // Constructor
  // -------------------------

  AppState() {
    scannerMode = NORMAL;
    previousScanMode = scannerMode;
    currentTracks = {};

    initDefaults();
  }

  void reset() { initDefaults(); }

  // -------------------------
  // LEDs
  // -------------------------

  bool getLed1State() const { return led1State; }

  void setLed1State(bool state) { led1State = state; }

  bool getLed2State() const { return led2State; }

  void setLed2State(bool state) { led2State = state; }

  // -------------------------
  // Automatic mode
  // -------------------------

  bool getAutoState() const { return autoState; }

  void setAutoState(bool state) { autoState = state; }

  AutoMode getAutoMode() const { return autoMode; }

  void setAutoMode(AutoMode mode) { autoMode = mode; }

  // -------------------------
  // Scanner mode
  // -------------------------
  ScannerMode getScannerMode() { return scannerMode; }
  void setScannerMode(ScannerMode newScannerMode) {
    previousScanMode = scannerMode;
    scannerMode = newScannerMode;
  }
  ScannerMode getPreviousScannerMode() { return previousScanMode; }

  void setScannerStatusLabelText(String scannerStatusLabelText_) {
    // Serial.print("Scanner status ");
    // Serial.println(scannerStatusLabelText_);
    scannerStatusLabelText = scannerStatusLabelText_;
  }
  String getScannerStatusLabelText() { return scannerStatusLabelText; }

  void setPathFinderStatusText(String pathFinderStatusText_) {
    // Serial.print("Path Finder status ");
    // Serial.println(pathFinderStatusText_);
    pathFinderStatusText = pathFinderStatusText_;
  }
  String getPathFinderStatusText() { return pathFinderStatusText; }

  bool getAmbianceAudioMode() const { return ambianceAudioMode; }

  void setAmbianceAudioMode(bool state) { ambianceAudioMode = state; }

  bool getMovementAudioMode() const { return movementAudioMode; }

  void setMovementAudioMode(bool state) { movementAudioMode = state; }

  bool getAudioState() const { return movementAudioMode && ambianceAudioMode; }

  void setAudioState(bool state) {
    movementAudioMode = state;
    ambianceAudioMode = state;
  }

  void setMovementAudioPitch(float pitch) { movementAudioPitch = pitch; }
  void setMovementAudioSpeed(float speed) { movementAudioSpeed = speed; }
  void setAmbianceAudioPitch(float pitch) { ambianceAudioPitch = pitch; }
  void setAmbianceAudioSpeed(float speed) { ambianceAudioSpeed = speed; }

  float getMovementAudioPitch() { return movementAudioPitch; }
  float getMovementAudioSpeed() { return movementAudioSpeed; }
  float getAmbianceAudioPitch() { return ambianceAudioPitch; }
  float getAmbianceAudioSpeed() { return ambianceAudioSpeed; }

  // Speed

  int getTargetSpeed() const { return targetSpeed; }

  void setTargetSpeed(int speed) { targetSpeed = speed; }

  int getUserSpeed() const { return userSpeed; }

  void setUserSpeed(int speed) { userSpeed = speed; }
  // std::function<void(lv_event_t*)> setUserSpeedLambda = [this](lv_event_t*
  // event) {
  //   setUserSpeed(static_cast<int>(value));
  // };

  // std::function<float()> getUserSpeedLambda = [this] {
  //   return static_cast<float>(getUserSpeed());
  // };
  std::function<const char*()> getUserSpeedLabelLambda = [this] {
    static char text[32];
    std::snprintf(text, sizeof(text), "Speed: %d", abs(getUserSpeed()));
    return text;
  };

  float getCurrentSpeed() const { return currentSpeed; }

  void setCurrentSpeed(float speed) { currentSpeed = speed; }

  // -------------------------
  // Ramp
  // -------------------------

  unsigned long getRampTime() const { return rampTime; }

  void setRampTime(unsigned long time) { rampTime = time; }

  std::function<const char*()> getRampTimeLabelLambda = [this] {
    static char text[32];
    std::snprintf(text, sizeof(text), "Ramp: %lu", getRampTime() / 1000);
    return text;
  };

  unsigned long getRampStartTime() const { return rampStartTime; }

  void setRampStartTime(unsigned long time) { rampStartTime = time; }

  int getRampStartSpeed() const { return rampStartSpeed; }

  void setRampStartSpeed(int speed) { rampStartSpeed = speed; }

  // -------------------------
  // Speed
  // -------------------------

  int getLastTargetSpeed() const { return lastTargetSpeed; }

  void setLastTargetSpeed(int speed) { lastTargetSpeed = speed; }

  // template track
  void setTemplateTrack(const Track& track) {
    // Serial.println("set template");
    templateTrack_ = track;
  }

  Track* getTemplateTrack() { return &templateTrack_; }

  const Track* getTemplateTrack() const { return &templateTrack_; }

  TrackType getSelectedTrackType() { return selectedTrackType; }
  void setSelectedTrackType(TrackType selectedTrackType_) {
    previousSelectedTrackType = selectedTrackType;
    selectedTrackType = selectedTrackType_;
  }
  TrackType getPreviousSelectedTrackType() { return previousSelectedTrackType; }

  void setPastTracks(const Tracks pastTracks_) {
    pastTracks = pastTracks_;
    hasPastTracks = true;
  }

  Tracks getPastTracks() { return pastTracks; }

  const Tracks getPastTracks() const { return pastTracks; }

  Tracks* getCurrentTracks() { return &currentTracks; }

  // const Tracks* getCurrentTracks() const { return &currentTracks; }
  void setPath(std::vector<Uid> path_) { path = path_; }
  std::vector<Uid>& getPath() {
    return path;
  }  // -------------------------
     // display
     // -------------------------

  String getStatusLabelText() { return statusLabelText; }
  void setStatusLabelText(String statusLabelText_) {
    statusLabelText = statusLabelText_;
    Serial.println(statusLabelText);
  }

  Uid getDestinationUid() { return destinationUid; };
  void setDestinationUid(Uid destinationUid_) {
    destinationUid = destinationUid_;
    // Serial.println("destinationUid");
    // if (destinationUid.isValid()) Serial.println(destinationUid.toString());
  };

  Uid getCurrentUid() { return currentUid; };
  void setCurrentUid(Uid currentUid_) { currentUid = currentUid_; };
  // settings
  int getMinPwm() { return minPwm; };
  void setMinPwm(int minPwm_) {
    minPwm = minPwm_;
    Serial.println(minPwm);
  };
  std::function<void(float)> setMinPwmLambda = [this](float value) {
    setMinPwm(static_cast<int>(value));
  };

  std::function<float()> getMinPwmLambda = [this] {
    return static_cast<float>(getMinPwm());
  };

  std::function<const char*()> getMinPwmLabelLambda = [this] {
    static char text[32];
    std::snprintf(text, sizeof(text), "Min PWM: %d", abs(getMinPwm()));
    return text;
  };

  float getLowAudioOffset() { return lowAudioOffset; };
  void setLowAudioOffset(float lowAudioOffset_) {
    lowAudioOffset = lowAudioOffset_;
  };

  std::function<void(float)> setLowAudioOffsetLambda = [this](float value) {
    setLowAudioOffset(value);
  };

  std::function<float()> getLowAudioOffsetLambda = [this] {
    return static_cast<float>(getLowAudioOffset());
  };

  std::function<const char*()> getLowAudioOffsetLabelLambda = [this] {
    static char text[32];
    std::snprintf(text, sizeof(text), "Low Offset: %.2f",
                  abs(getLowAudioOffset()));
    return text;
  };

  float getHighAudioOffset() { return highAudioOffset; };
  void setHighAudioOffset(float highAudioOffset_) {
    highAudioOffset = highAudioOffset_;
  };

  std::function<void(float)> setHighAudioOffsetLambda = [this](float value) {
    setHighAudioOffset(value);
  };

  std::function<float()> getHighAudioOffsetLambda = [this] {
    return static_cast<float>(getHighAudioOffset());
  };

  std::function<const char*()> getHighAudioOffsetLabelLambda = [this] {
    static char text[32];
    std::snprintf(text, sizeof(text), "High Offset: %.2f",
                  abs(getHighAudioOffset()));
    return text;
  };

  float getAmbianceSoundVolume() { return ambianceSoundVolume; };
  void setAmbianceSoundVolume(float ambianceSoundVolume_) {
    ambianceSoundVolume = ambianceSoundVolume_;
  };

  std::function<void(float)> setAmbianceSoundVolumeLambda =
      [this](float value) { setAmbianceSoundVolume(value); };

  std::function<float()> getAmbianceSoundVolumeLambda = [this] {
    return static_cast<float>(getAmbianceSoundVolume());
  };

  std::function<const char*()> getAmbianceSoundVolumeLabelLambda = [this] {
    static char text[32];
    std::snprintf(text, sizeof(text), "Ambiance Vol: %.2f",
                  abs(getAmbianceSoundVolume()));
    return text;
  };

  float getMovementSoundVolume() { return movementSoundVolume; };
  void setMovementSoundVolume(float movementSoundVolume_) {
    movementSoundVolume = movementSoundVolume_;
  };

  std::function<void(float)> setMovementSoundVolumeLambda =
      [this](float value) { setMovementSoundVolume(value); };

  std::function<float()> getMovementSoundVolumeLambda = [this] {
    return static_cast<float>(getMovementSoundVolume());
  };

  std::function<const char*()> getMovementSoundVolumeLabelLambda = [this] {
    static char text[32];
    std::snprintf(text, sizeof(text), "Movement Vol: %.2f",
                  abs(getMovementSoundVolume()));
    return text;
  };

 private:
  void initDefaults() {
    Serial.println("reset app");
    // LEDs
    led1State = false;
    led2State = false;

    // Automatic mode
    autoState = false;
    autoMode = AUTO_IDLE;

    // Scanner
    scannerMode = NORMAL;
    previousScanMode = NORMAL;

    scannerStatusLabelText = "";
    pathFinderStatusText = "";

    // Audio
    ambianceAudioMode = false;
    movementAudioMode = true;

    movementAudioPitch = 0;
    movementAudioSpeed = 0;

    ambianceAudioPitch = 0;
    ambianceAudioSpeed = 0;

    // Speed

    userSpeed = 0;

    // Ramp
    rampTime = 0;

    // template track
    Track templateTrack_;
    hasTemplateTrack_ = false;

    selectedTrackType = CURVED;
    previousSelectedTrackType = CURVED;

    currentTracks.clear();
    currentTracks = {};

    pastTracks.clear();
    pastTracks = {};
    hasPastTracks = false;

    // display status label
    statusLabelText = "";

    // location

    destinationUid.clear();
    currentUid.clear();

    // Path find
    path.clear();

    // settings
    minPwm = 0;
    lowAudioOffset = 0;
    highAudioOffset = 0;
    ambianceSoundVolume = 0;
    movementSoundVolume = 0;
  }

  // LEDs
  bool led1State;
  bool led2State;

  // Automatic mode
  bool autoState;
  AutoMode autoMode;

  // Scanner
  ScannerMode scannerMode;
  ScannerMode previousScanMode;

  String scannerStatusLabelText;
  // path find
  String pathFinderStatusText;

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

  Tracks pastTracks;
  bool hasPastTracks = false;

  Tracks currentTracks;

  // Path finding
  std::vector<Uid> path;

  // display status label
  String statusLabelText;

  // location
  Uid destinationUid;
  Uid currentUid;

  // settings
  int minPwm;
  float lowAudioOffset;
  float highAudioOffset;
  float ambianceSoundVolume;

  float movementSoundVolume;
};
extern AppState app;

#endif  // APP_STATE_HPP
