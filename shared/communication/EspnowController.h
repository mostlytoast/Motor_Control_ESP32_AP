#ifndef EspnowController_H
#define EspnowController_H
// TODO fix issue where it does not always pair (like when turning controller
// off and on after paired)
// TODO unify naming scheme
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdint.h>

#include "../SharedConfig.h"
#include "../utils/AppState.h"
#include "../utils/MacUtils.h"
#include "JsonAssembler.h"
#include "StateJson.h"
#include "packets.h"

// ESP-NOW TIMING
#define HEARTBEAT_INTERVAL 350
#define DISCOVERY_INTERVAL 1000
#define DISCOVERY_TIMEOUT 5000
#define MAX_RECEIVERS 8

// =====================================================
// RECEIVER STATE
// =====================================================

struct ReceiverInfo {
  bool used;
  uint8_t mac[6];
  char name[24];
  uint8_t channel;
  bool paired;
  unsigned long lastSeen;
};

// =====================================================
// PAIRING STATES
// =====================================================

enum validateSelectedPeerState {
  INVALID_INDEX,
  NOT_USED,
  ALREADY_SELECTED,
  PEER_ERROR,
  PASS_CHECK
};

enum sendPairRequestState { PAIR_ERROR, PAIR_SUCCESS };

// =====================================================
// ESP-NOW EVENT QUEUE
// =====================================================

#define ESPNOW_EVENT_DATA_MAX 250
#define ESPNOW_EVENT_QUEUE_SIZE 16

enum EspNowEventType : uint8_t {
  ESP_NOW_EVENT_RECEIVE = 0,
  ESP_NOW_EVENT_SEND = 1
};

struct EspNowEvent {
  EspNowEventType type;
  uint8_t mac[6];
  uint8_t data[ESPNOW_EVENT_DATA_MAX];
  uint16_t len;
  esp_now_send_status_t sendStatus;
};

class EspnowController {
 public:
  // Event Queue
  void queueEspNowEvent(EspNowEventType type, const uint8_t* mac,
                        const uint8_t* data, int len);

  // Receiver management

  void removePeers();
  // Initialization
  bool setupESPNow(bool wifi = false);

  // Periodic communication

  void espnowUpdate();

  // Pairing
  sendPairRequestState sendPairRequest(int index);

  validateSelectedPeerState validateSelectedPeer(int index);

  // Discovery management
  void resetDiscover();
  bool hasActiveReceiver();
  bool isActiveReceiver(int i);

  ReceiverInfo* getReceivers();
  int getActiveReceiver();
  int getNumberOfReceivers();
  void setReceiverListNeedsRefresh(bool enable);
  bool getReceiverListNeedsRefresh();

 private:
  bool sendControllerClaim(int receiverIndex);
  bool sendJsonDocument(const uint8_t* destination, JsonDocument& doc);
  bool processJsonFragment(const uint8_t* sourceMac, const uint8_t* data,
                           size_t len);
  void processJsonCommand(const uint8_t* sourceMac, JsonDocument& doc);
  // Receiver management
  int findController(const uint8_t* mac);

  bool addPeer(const uint8_t* mac, uint8_t channel);

  void removePeer(const uint8_t* mac);
  bool removeController(int slot);

  int addOrUpdateReceiver(const DiscoveryPacket* packet);
  bool pairWithReceiver(int index);

  // JSON communication
  bool sendJsonDocument(JsonDocument& doc, const uint8_t* mac);

  // ESP-NOW commands
  bool sendCommand(uint8_t type, int16_t value);

  bool sendServerState(const uint8_t* destination);

  // Discovery
  void sendDiscovery();

  void processDiscovery();

  void ESPNow_updatePairingMode();

  void sendDiscoveryPacket(const uint8_t* destination);

  // Periodic communication
  void sendHeartbeat();

  // Incoming state
  void processServerState(JsonDocument& doc);
  void processESPNowEvents();

  static void onESPNowReceive(const esp_now_recv_info_t* info,
                              const uint8_t* data, int len);
  static void onESPNowSendActual(const wifi_tx_info_t* tx_info,
                                 esp_now_send_status_t status);

  static EspnowController* callbackInstance;
  void ESPNow_checkFailsafe();

  bool isPairedController(const uint8_t* mac);

  int findFreeControllerSlot();

  int pairController(const uint8_t* mac, const char* controllerName);

  bool claimController(const uint8_t* mac);
  bool isActiveController(const uint8_t* mac);

  void sendPairResponse(const uint8_t* destination, int slot, bool paired);

  void sendControllerStatus(const uint8_t* destination);

  bool processReceiverBinaryPacket(const uint8_t* sourceMac,
                                   const uint8_t* data, int len);

  bool processControllerBinaryPacket(const uint8_t* sourceMac,
                                     const uint8_t* data, int len);
  bool hasPreviousStateChanged();
  void processStateChanges();
  // =====================================================
  // LOCAL ESP-NOW STATE
  // =====================================================

  uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  uint8_t localMac[6] = {};

  QueueHandle_t espNowEventQueue = nullptr;

  // =====================================================
  // CONTROLLER-SIDE RECEIVER LIST
  // =====================================================

  int receiverCount = 0;
  int activeReceiver = -1;

  volatile bool receiverListNeedsRefresh = false;

  ReceiverInfo receivers[MAX_RECEIVERS] = {};

  // =====================================================
  // DISCOVERY STATE
  // =====================================================

  bool discoveryActive = false;

  unsigned long discoveryStarted = 0;
  unsigned long lastDiscoverySend = 0;

  // =====================================================
  // CONTROLLER-SIDE SERVER STATE
  // =====================================================

  JsonDocument serverState;

  // =====================================================
  // PERIODIC COMMUNICATION
  // =====================================================

  unsigned long lastHeartbeatSend = 0;

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
};
extern EspnowController espnowController;

#endif  // EspnowController_H