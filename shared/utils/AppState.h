#pragma once

#include <Arduino.h>

#include "../SharedConfig.h"
#include "../communication/packets.h"
#include "../track/map.h"

// extern bool serverStateReceived;

// extern bool serverStateChanged;

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
  // TODO figure out when to call these
  bool getServerStateChanged();
  void setServerStateChanged(bool state);

  bool getServerStateReceived();
  void setServerStateReceived(bool state);
  // -------------------------
  // LEDs
  // -------------------------

  bool getLed1State() const;
  void setLed1State(bool state);

  bool getLed2State() const;
  void setLed2State(bool state);

  // -------------------------
  // Automatic mode
  // -------------------------

  bool getAutoState() const;
  void setAutoState(bool state);

  AutoMode getAutoMode() const;
  void setAutoMode(AutoMode mode);

  // -------------------------
  // Scanner mode
  // -------------------------

  ScannerMode getScannerMode();
  void setScannerMode(ScannerMode mode);

  ScannerMode getPreviousScannerMode();

  // -------------------------
  // Audio mode
  // -------------------------

  bool getAmbianceAudioMode() const;
  void setAmbianceAudioMode(bool state);

  bool getMovementAudioMode() const;
  void setMovementAudioMode(bool state);

  bool getAudioState() const;
  void setAudioState(bool state);

  // -------------------------
  // Controllers
  // -------------------------

  PairedController* getControllers();

  int getActiveController() const;
  void setActiveController(int controller);

  unsigned long getLastActiveControllerPacket() const;
  void setLastActiveControllerPacket(unsigned long time);

  // -------------------------
  // ESP-NOW
  // -------------------------

  bool getEspNowCommandPending() const;
  void setEspNowCommandPending(bool pending);

  void setEspNowPendingCommand(ESPNowCommand&);
  ESPNowCommand& getEspNowPendingCommand();

  uint8_t* getPendingSourceMac();
  void setPendingSourceMac(const uint8_t* mac);

  unsigned long getLastESPNowPacket() const;
  void setLastESPNowPacket(unsigned long time);

  bool getEspNowFailsafeActive() const;
  void setEspNowFailsafeActive(bool active);

  // -------------------------
  // Pairing
  // -------------------------

  bool getPairingMode() const;
  void setPairingMode(bool enabled);

  unsigned long getPairingModeStarted() const;
  void setPairingModeStarted(unsigned long time);

  // -------------------------
  // NFC
  // -------------------------

  void setUid(const uint8_t* uid, uint8_t length);

  const uint8_t* getUid() const;

  void setUidLength(uint8_t length);

  uint8_t getUidLength() const;

  // -------------------------
  // Speed
  // -------------------------

  int getTargetSpeed() const;
  void setTargetSpeed(int speed);

  int getUserSpeed() const;
  void setUserSpeed(int speed);

  float getCurrentSpeed() const;
  void setCurrentSpeed(float speed);

  // -------------------------
  // Ramp
  // -------------------------

  unsigned long getRampTime() const;
  void setRampTime(unsigned long time);

  unsigned long getRampStartTime() const;
  void setRampStartTime(unsigned long time);

  int getRampStartSpeed() const;
  void setRampStartSpeed(int speed);

  int getLastTargetSpeed() const;
  void setLastTargetSpeed(int speed);

  // -------------------------
  // Sensors
  // -------------------------

  unsigned long getLastSensorDetect() const;
  void setLastSensorDetect(unsigned long time);

  int getSpeedBeforeDetect() const;
  void setSpeedBeforeDetect(int speed);

  Track* getTemplateTrack();
  void setTemplateTrack(Track* templateTrack_);

 private:
  bool serverStateChanged = false;
  bool serverStateReceived = false;
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

  // Controllers
  PairedController controllers[MAX_CONTROLLERS];
  int activeController = -1;
  unsigned long lastActiveControllerPacket = 0;

  // ESP-NOW
  volatile bool espNowCommandPending = false;
  ESPNowCommand espNowPendingCommand;
  uint8_t pendingSourceMac[6] = {};
  volatile unsigned long lastESPNowPacket = 0;
  bool espNowFailsafeActive = false;

  // Pairing
  bool pairingMode = true;
  unsigned long pairingModeStarted = 0;

  // NFC
  uint8_t uidLength = 0;
  uint8_t uid[7] = {};

  // Speed
  volatile int targetSpeed = 0;
  volatile int userSpeed = 0;
  float currentSpeed = 0.0;

  // Ramp
  unsigned long rampTime = 0;
  unsigned long rampStartTime = 0;
  int rampStartSpeed = 0;
  int lastTargetSpeed = 0;

  // Sensors
  unsigned long lastSensorDetect = 0;
  int speedBeforeDetect = 200;
  Track* templateTrack = nullptr;
};

extern AppState app;