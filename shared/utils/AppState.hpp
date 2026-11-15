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

  float getCurrentSpeed() const { return currentSpeed; }

  void setCurrentSpeed(float speed) { currentSpeed = speed; }

  // -------------------------
  // Ramp
  // -------------------------

  unsigned long getRampTime() const { return rampTime; }

  void setRampTime(unsigned long time) { rampTime = time; }

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

  void setPastTracks(const Tracks& pastTracks_) { pastTracks = pastTracks_; }

  Tracks& getPastTracks() { return pastTracks; }

  const Tracks& getPastTracks() const { return pastTracks; }

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
  }

  Uid getDestinationUid() { return destinationUid; };
  void setDestinationUid(Uid destinationUid_) {
    destinationUid = destinationUid_;
    // Serial.println("destinationUid");
    // if (destinationUid.isValid()) Serial.println(destinationUid.toString());
  };

  Uid getCurrentUid() { return currentUid; };
  void setCurrentUid(Uid currentUid_) { currentUid = currentUid_; };

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
    targetSpeed = 0;
    userSpeed = 0;
    currentSpeed = 0.0;

    // Ramp
    rampTime = 0;
    rampStartTime = 0;
    rampStartSpeed = 0;
    lastTargetSpeed = 0;

    // template track
    Track templateTrack_;
    hasTemplateTrack_ = false;

    selectedTrackType = CURVED;
    previousSelectedTrackType = CURVED;

    currentTracks.clear();
    currentTracks = {};

    // display status label
    statusLabelText = "";

    // location

    destinationUid.clear();
    currentUid.clear();

    // Path find
    path.clear();
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

  Tracks currentTracks;

  // Path finding
  std::vector<Uid> path;

  // display status label
  String statusLabelText;

  // location
  Uid destinationUid;
  Uid currentUid;
};
extern AppState app;

#endif  // APP_STATE_HPP
