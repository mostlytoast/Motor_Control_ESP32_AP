#include "AppState.h"

AppState app;

// -------------------------
// Constructor
// -------------------------

AppState::AppState() {
  scannerMode = SCANNING_TRACKS;
  previousScanMode = scannerMode;
}

// -------------------------
// LEDs
// -------------------------
bool AppState::getServerStateChanged() { return serverStateChanged; }

void AppState::setServerStateChanged(bool state) { serverStateChanged = state; }

bool AppState::getServerStateReceived() { return serverStateReceived; }

void AppState::setServerStateReceived(bool state) {
  serverStateReceived = state;
}
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
// -------------------------
// Audio mode
// -------------------------

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
// -------------------------
// Controllers
// -------------------------

PairedController* AppState::getControllers() { return controllers; }

int AppState::getActiveController() const { return activeController; }

void AppState::setActiveController(int controller) {
  activeController = controller;
}

unsigned long AppState::getLastActiveControllerPacket() const {
  return lastActiveControllerPacket;
}

void AppState::setLastActiveControllerPacket(unsigned long time) {
  lastActiveControllerPacket = time;
}

// -------------------------
// ESP-NOW
// -------------------------

bool AppState::getEspNowCommandPending() const { return espNowCommandPending; }

void AppState::setEspNowCommandPending(bool pending) {
  espNowCommandPending = pending;
}

void AppState::setEspNowPendingCommand(ESPNowCommand& newEspNowPendingCommand) {
  espNowPendingCommand = newEspNowPendingCommand;
}
ESPNowCommand& AppState::getEspNowPendingCommand() {
  return espNowPendingCommand;
}

uint8_t* AppState::getPendingSourceMac() { return pendingSourceMac; }

void AppState::setPendingSourceMac(const uint8_t* mac) {
  if (mac == nullptr) {
    memset(pendingSourceMac, 0, sizeof(pendingSourceMac));
    return;
  }

  memcpy(pendingSourceMac, mac, sizeof(pendingSourceMac));
}

unsigned long AppState::getLastESPNowPacket() const { return lastESPNowPacket; }

void AppState::setLastESPNowPacket(unsigned long time) {
  lastESPNowPacket = time;
}

bool AppState::getEspNowFailsafeActive() const { return espNowFailsafeActive; }

void AppState::setEspNowFailsafeActive(bool active) {
  espNowFailsafeActive = active;
}

// -------------------------
// Pairing
// -------------------------

bool AppState::getPairingMode() const { return pairingMode; }

void AppState::setPairingMode(bool enabled) { pairingMode = enabled; }

unsigned long AppState::getPairingModeStarted() const {
  return pairingModeStarted;
}

void AppState::setPairingModeStarted(unsigned long time) {
  pairingModeStarted = time;
}

// -------------------------
// NFC
// -------------------------

void AppState::setUid(const uint8_t* newUid, uint8_t length) {
  if (newUid == nullptr) {
    memset(uid, 0, sizeof(uid));
    uidLength = 0;
    return;
  }

  if (length > sizeof(uid)) {
    length = sizeof(uid);
  }

  memset(uid, 0, sizeof(uid));
  memcpy(uid, newUid, length);

  uidLength = length;
}

const uint8_t* AppState::getUid() const { return uid; }

void AppState::setUidLength(uint8_t length) {
  if (length > sizeof(uid)) {
    length = sizeof(uid);
  }

  uidLength = length;
}

uint8_t AppState::getUidLength() const { return uidLength; }

// -------------------------
// Speed
// -------------------------

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

int AppState::getLastTargetSpeed() const { return lastTargetSpeed; }

void AppState::setLastTargetSpeed(int speed) { lastTargetSpeed = speed; }

// -------------------------
// Sensors
// -------------------------

unsigned long AppState::getLastSensorDetect() const { return lastSensorDetect; }

void AppState::setLastSensorDetect(unsigned long time) {
  lastSensorDetect = time;
}

int AppState::getSpeedBeforeDetect() const { return speedBeforeDetect; }

void AppState::setSpeedBeforeDetect(int speed) { speedBeforeDetect = speed; }

Track* AppState::getTemplateTrack() { return templateTrack; }
void AppState::setTemplateTrack(Track* templateTrack_) {
  templateTrack = templateTrack_;
}