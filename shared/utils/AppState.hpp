#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <Arduino.h>

#include <vector>

#include "../track/Point.hpp"
#include "../track/Tag.hpp"
#include "../track/Tracks.hpp"
#include "Uids.hpp"
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

  std::function<void()> toggleLed1StateLambda = [this]() {
    setLed1State(!getLed1State());
  };

  std::function<bool()> getLed1StateLambda = [this] {
    return getLed1State();
    ;
  };
  bool getLed2State() const { return led2State; }

  void setLed2State(bool state) { led2State = state; }

  std::function<void()> toggleLed2StateLambda = [this]() {
    setLed2State(!getLed2State());
  };
  std::function<bool()> getLed2StateLambda = [this] {
    return getLed2State();
    ;
  };
  // -------------------------
  // Automatic mode
  // -------------------------

  bool getAutoState() const { return autoState; }

  void setAutoState(bool state) { autoState = state; }

  std::function<void()> toggleAutoStateLambda = [this]() {
    setAutoState(!getAutoState());
  };

  std::function<bool()> getAutoStateLambda = [this] {
    return getAutoState();
    ;
  };
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

  std::function<bool()> getScannerModeNormalLambda = [this] {
    return getScannerMode() == NORMAL;
  };
  std::function<bool()> getScannerModeRegisterTracksLambda = [this] {
    return getScannerMode() == REGISTER_TRACKS;
  };
  std::function<bool()> getScannerModeWritingTracksLambda = [this] {
    return getScannerMode() == WRITING_TRACKS;
  };
  std::function<bool()> getScannerModeScanningTracksLambda = [this] {
    return getScannerMode() == SCANNING_TRACKS;
  };
  std::function<bool()> getScannerModePathFindingLambda = [this] {
    return getScannerMode() == PATH_FINDING;
  };

  std::function<void()> setScannerModeNormalLambda = [this]() {
    setScannerMode(NORMAL);
  };
  std::function<void()> setScannerModeRegisterTracksLambda = [this]() {
    setScannerMode(REGISTER_TRACKS);
  };
  std::function<void()> setScannerModeWritingTracksLambda = [this]() {
    setScannerMode(WRITING_TRACKS);
  };
  std::function<void()> setScannerModeScanningTracksLambda = [this]() {
    setScannerMode(SCANNING_TRACKS);
  };
  std::function<void()> setScannerModePathFindingLambda = [this]() {
    setScannerMode(PATH_FINDING);
  };
  ScannerMode getPreviousScannerMode() { return previousScanMode; }

  void setScannerStatusLabelText(String scannerStatusLabelText_) {
    // Serial.print("Scanner status ");
    // Serial.println(scannerStatusLabelText_);
    scannerStatusLabelText = scannerStatusLabelText_;
  }
  String getScannerStatusLabelText() { return scannerStatusLabelText; }

  std::function<const char*()> getScannerStatusLabelLambda = [this] {
    // if (!getScannerStatusLabelText().equals("")) {
    static char text[32];

    std::snprintf(text, sizeof(text), "%s",
                  getScannerStatusLabelText().c_str());

    return text;
    // }
  };
  void setPathFinderStatusText(String pathFinderStatusText_) {
    // Serial.print("Path Finder status ");
    // Serial.println(pathFinderStatusText_);
    pathFinderStatusText = pathFinderStatusText_;
  }
  String getPathFinderStatusText() { return pathFinderStatusText; }

  std::function<const char*()> getPathFinderStatusLambda = [this] {
    if (!getPathFinderStatusText().equals("")) {
      static char text[32];

      std::snprintf(text, sizeof(text), "%s",
                    getPathFinderStatusText().c_str());

      return text;
    } else {
      static char text[32];

      std::snprintf(text, sizeof(text), "none");

      return text;
    }
  };
  bool getAmbianceAudioState() const { return ambianceAudioState; }

  void setAmbianceAudioState(bool state) { ambianceAudioState = state; }

  std::function<void()> toggleAmbianceAudioStateLambda = [this]() {
    setAmbianceAudioState(!getAmbianceAudioState());
  };

  std::function<bool()> getAmbianceAudioStateLambda = [this] {
    return getAmbianceAudioState();
    ;
  };

  bool getMovementAudioState() const { return movementAudioState; }

  void setMovementAudioState(bool state) { movementAudioState = state; }

  bool getAudioState() const {
    return movementAudioState && ambianceAudioState;
  }

  void setAudioState(bool state) {
    movementAudioState = state;
    ambianceAudioState = state;
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

  std::function<void()> increaseUserSpeedLambda = [this]() {
    setUserSpeed(clampInt(getUserSpeed() + 10, -MAX_PWM, MAX_PWM));
  };

  std::function<void()> setUserSpeedZeroLambda = [this]() { setUserSpeed(0); };
  std::function<void()> decreaseUserSpeedLambda = [this]() {
    setUserSpeed(clampInt(getUserSpeed() - 10, -MAX_PWM, MAX_PWM));
  };
  std::function<float()> getUserSpeedLambda = [this] {
    return static_cast<float>(getUserSpeed());
  };
  std::function<const char*()> getUserSpeedLabelLambda = [this] {
    static char text[32];
    std::snprintf(text, sizeof(text), "Speed: %d", abs(getUserSpeed()));
    return text;
  };

  float getCurrentSpeed() const { return currentSpeed; }

  void setCurrentSpeed(float speed) { currentSpeed = speed; }

  std::function<void()> increaseRampTimeLambda = [this]() {
    setRampTime(clampInt(getRampTime() + kRampStep, kRampMin, kRampMax));
  };

  std::function<void()> decreaseRampTimeLambda = [this]() {
    setRampTime(clampInt(getRampTime() - kRampStep, kRampMin, kRampMax));
  };
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

  Tracks* getCurrentTracks() { return &currentTracks; }
  std::function<const char*()> getTrackCountLabelLambda = [this] {
    static char text[32];
    snprintf(text, sizeof(text), "Tracks: %u",
             static_cast<unsigned>(getCurrentTracks()->size()));
    return text;
  };
  // const Tracks* getCurrentTracks() const { return &currentTracks; }
  void setPath(Uids path_) { path = path_; }
  Uids& getPath() {
    return path;
  }  // -------------------------
     // display
     // -------------------------

  String getStatusLabelText() { return statusLabelText; }
  void setStatusLabelText(String statusLabelText_) {
    statusLabelText = statusLabelText_;
  }
  std::function<const char*()> getStatusLabelLambda = [this] {
    // if (!getStatusLabelText().equals("")) {
    static char text[32];

    std::snprintf(text, sizeof(text), "%s", getStatusLabelText().c_str());

    return text;
    // }
  };

  Uid getDestinationUid() { return destinationUid; };
  void setDestinationUid(Uid destinationUid_) {
    destinationUid = destinationUid_;
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
    ambianceAudioState = false;
    movementAudioState = true;

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

    // display status label
    statusLabelText = "NO PEER";

    // location

    destinationUid.clear();
    currentUid.clear();

    // Path find
    path.clear();

    // settings
    minPwm = 50;
    lowAudioOffset = 0.0f;
    highAudioOffset = 0.0f;
    ambianceSoundVolume = 0.25;
    movementSoundVolume = 1.0;
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
  bool ambianceAudioState;
  bool movementAudioState;

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

  Tracks currentTracks;

  // Path finding
  Uids path;

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
