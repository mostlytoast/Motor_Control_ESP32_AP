#include "AppState.h"

AppState app;

// -------------------------
// Constructor
// -------------------------

AppState::AppState() {
  scannerMode = REGISTER_TRACKS;
  previousScanMode = scannerMode;
}

void AppState::reset()  { initDefaults(); }

void AppState::initDefaults() {
  Serial.println("reset app");
  // LEDs
  led1State = false;
  led2State = false;

  // Automatic mode
  autoState = false;
  autoMode = AUTO_IDLE;

  // Scanner
  scannerMode = REGISTER_TRACKS;
  previousScanMode = REGISTER_TRACKS;

  ScannerStatusLabelText = "";

  // Audio
  ambianceAudioMode = false;
  movementAudioMode = false;

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
  // display status label
  statusLabelText = "";

  // location

  destinationUid.clear();
  currentUid.clear();
}

// -------------------------
// LEDs
// -------------------------

bool AppState::getLed1State() const { return led1State; }

void AppState::setLed1State(bool state) { led1State = state; }

bool AppState::getLed2State() const { return led2State; }

void AppState::setLed2State(bool state) { led2State = state; }

// -------------------------
// Automatic mode
// -------------------------

bool AppState::getAutoState() const { return autoState; }

void AppState::setAutoState(bool state) { autoState = state; }

AutoMode AppState::getAutoMode() const { return autoMode; }

void AppState::setAutoMode(AutoMode mode) { autoMode = mode; }

// -------------------------
// Scanner mode
// -------------------------
ScannerMode AppState::getScannerMode() { return scannerMode; }
void AppState::setScannerMode(ScannerMode newScannerMode) {
  previousScanMode = scannerMode;
  scannerMode = newScannerMode;
}
ScannerMode AppState::getPreviousScannerMode() { return previousScanMode; }

void AppState::setScannerStatusLabelText(String ScannerStatusLabelText_) {
  Serial.print("Scanner status ");
  Serial.println(ScannerStatusLabelText_);
  ScannerStatusLabelText = ScannerStatusLabelText_;
}
String AppState::getScannerStatusLabelText() { return ScannerStatusLabelText; }

// Audio mode

bool AppState::getAmbianceAudioMode() const { return ambianceAudioMode; }

void AppState::setAmbianceAudioMode(bool state) { ambianceAudioMode = state; }

bool AppState::getMovementAudioMode() const { return movementAudioMode; }

void AppState::setMovementAudioMode(bool state) { movementAudioMode = state; }

bool AppState::getAudioState() const {
  return movementAudioMode && ambianceAudioMode;
}

void AppState::setAudioState(bool state) {
  movementAudioMode = state;
  ambianceAudioMode = state;
}

void AppState::setMovementAudioPitch(float pitch) {
  movementAudioPitch = pitch;
}
void AppState::setMovementAudioSpeed(float speed) {
  movementAudioSpeed = speed;
}
void AppState::setAmbianceAudioPitch(float pitch) {
  ambianceAudioPitch = pitch;
}
void AppState::setAmbianceAudioSpeed(float speed) {
  ambianceAudioSpeed = speed;
}

float AppState::getMovementAudioPitch() { return movementAudioPitch; }
float AppState::getMovementAudioSpeed() { return movementAudioSpeed; }
float AppState::getAmbianceAudioPitch() { return ambianceAudioPitch; }
float AppState::getAmbianceAudioSpeed() { return ambianceAudioSpeed; }

// Speed

int AppState::getTargetSpeed() const { return targetSpeed; }

void AppState::setTargetSpeed(int speed) { targetSpeed = speed; }

int AppState::getUserSpeed() const { return userSpeed; }

void AppState::setUserSpeed(int speed) { userSpeed = speed; }

float AppState::getCurrentSpeed() const { return currentSpeed; }

void AppState::setCurrentSpeed(float speed) { currentSpeed = speed; }

// -------------------------
// Ramp
// -------------------------

unsigned long AppState::getRampTime() const { return rampTime; }

void AppState::setRampTime(unsigned long time) { rampTime = time; }

unsigned long AppState::getRampStartTime() const { return rampStartTime; }

void AppState::setRampStartTime(unsigned long time) { rampStartTime = time; }

int AppState::getRampStartSpeed() const { return rampStartSpeed; }

void AppState::setRampStartSpeed(int speed) { rampStartSpeed = speed; }

// -------------------------
// Speed
// -------------------------

int AppState::getLastTargetSpeed() const { return lastTargetSpeed; }

void AppState::setLastTargetSpeed(int speed) { lastTargetSpeed = speed; }

// template track
void AppState::setTemplateTrack(const Track& track) {
  templateTrack_ = track;
  hasTemplateTrack_ = true;
}

Track* AppState::getTemplateTrack() {
  return hasTemplateTrack_ ? &templateTrack_ : nullptr;
}

const Track* AppState::getTemplateTrack() const {
  return hasTemplateTrack_ ? &templateTrack_ : nullptr;
}

TrackType AppState::getSelectedTrackType() { return selectedTrackType; }
void AppState::setSelectedTrackType(TrackType selectedTrackType_) {
  previousSelectedTrackType = selectedTrackType;
  selectedTrackType = selectedTrackType_;
}
TrackType AppState::getPreviousSelectedTrackType() {
  return previousSelectedTrackType;
}

// -------------------------
// display
// -------------------------

String AppState::getStatusLabelText() { return statusLabelText; }
void AppState::setStatusLabelText(String statusLabelText_) {
  statusLabelText = statusLabelText_;
}

Uid AppState::getDestinationUid() { return destinationUid; };
void AppState::setDestinationUid(Uid destinationUid_) {
  destinationUid = destinationUid_;
};

Uid AppState::getCurrentUid() { return currentUid; };
void AppState::setCurrentUid(Uid currentUid_) { currentUid = currentUid_; };