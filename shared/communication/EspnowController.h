#ifndef EspnowController_H
#define EspnowController_H
// TODO fix issue where it does not always pair (like when turning controller
// off and on after paired)
// TODO unify naming scheme
// TODO: Fix ESP-NOW pairing logic for correct claiming sequence.
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
#include "Peers.h"
#include "packets.h"

// ESP-NOW TIMING
#define HEARTBEAT_INTERVAL 350
#define DISCOVERY_INTERVAL 2000
#define DISCOVERY_TIMEOUT 5000
#define MAX_PEERS 5

// =====================================================
// RECEIVER STATE
// =====================================================

// struct ReceiverInfo {
//   bool used;
//   uint8_t mac[6];
//   char name[24];
//   uint8_t channel;
//   bool paired;
//   unsigned long lastSeen;
// };

// =====================================================
// PAIRING STATES
// =====================================================

enum validateSelectedPeerState {
  NOT_USED,
  ALREADY_SELECTED,
  PEER_ERROR,
  PASS_CHECK
};

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
  // Peer peer;
  uint8_t mac[6];

  uint8_t data[ESPNOW_EVENT_DATA_MAX];
  uint16_t len;
  esp_now_send_status_t sendStatus;
};

class EspnowController {
 public:
  using GetJsonFromStateCallback = void (*)(JsonDocument&);
  using GetStateFromJsonCallback = void (*)(JsonDocument&);
  using ResetStateCallback = std::function<void()>;

  // Event Queue
  void queueEspNowEvent(EspNowEventType type, const uint8_t* mac,
                        const uint8_t* data, int len);

  // Receiver management
  void removePeers();
  // ReceiverInfo* getPeers();
  // int getActiveReceiver();
  int getNumberOfReceivers();
  void setReceiverListNeedsRefresh(bool enable);
  bool getReceiverListNeedsRefresh();

  // Initialization
  bool setupESPNow(GetJsonFromStateCallback getJsonFromStateCallback_,
                   GetStateFromJsonCallback getStateFromJsonCallback_,
                   ResetStateCallback resetStateCallback_, bool wifi = false);

  // Periodic communication
  void espnowUpdate();

  // Pairing
  bool sendPairRequest(Peer* peer);

  validateSelectedPeerState validateSelectedPeer(Peer* peer);

  // Discovery management
  void resetDiscover();
  // bool isActiveReceiver(int i);
  bool hasActiveReceiver();
  std::vector<Peer> getActiveControllerPeers();
  std::vector<Peer> getActiveReceiverPeers();
  std::vector<Peer> getPeers();

  Peer* getActiveReceiverPeer();

 private:
  // Receiver management
  bool sendControllerClaim(Peer* peer);
  int findController(const uint8_t* mac);

  // bool addPeer(const uint8_t* mac, uint8_t channel);
  bool addPeer(const uint8_t* mac, uint8_t channel, const char* name);
  bool addPeer(Peer* peer);

  void removePeer(const uint8_t* mac);
  // bool removeController(int slot);

  bool addOrUpdateDiscoveredPeer(Peer* peer, const ESPNowCommand* packet);
  // bool pairWithReceiver(int index);

  bool isActiveController(const uint8_t* mac);

  // JSON communication
  
  bool sendCommandToPeer(Peer* peer, ESPNowCommand command);
  bool sendJsonDocument(Peer* peer, JsonDocument& doc);

  bool sendServerState(Peer* peer);
  // Discovery

  void processDiscovery();

  void sendDiscoveryResponse(Peer* peer);

  // Periodic communication
  void sendHeartbeat();

  // Incoming state
  void processServerState(JsonDocument& doc);
  void processESPNowEvents();

  void NewFunction(Peer* peer, EspNowEvent& event);

  static void onESPNowReceive(const esp_now_recv_info_t* info,
                              const uint8_t* data, int len);
  static void onESPNowSendActual(const wifi_tx_info_t* tx_info,
                                 esp_now_send_status_t status);

  static EspnowController* callbackInstance;
  void ESPNowCheckFailsafe();

  void sendPairResponse(Peer* peer);

  // Todo might wanna use peer over mac
  bool processReceivedPeerBinaryPacket(Peer* peer, const uint8_t* data,
                                       int len);

  bool processReceivedControllerBinaryPacket(Peer* peer, const uint8_t* data,
                                             int len);
  bool processReceivedJsonFragment(Peer* peer, const uint8_t* data, size_t len);

  void processStateChanges();

  void handleUnkownPacket(Peer* peer, EspNowEvent& event);
  // =====================================================
  // LOCAL ESP-NOW STATE
  // =====================================================

  uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  uint8_t localMac[6] = {};

  QueueHandle_t espNowEventQueue = nullptr;

  // =====================================================
  // CONTROLLER-SIDE RECEIVER LIST
  // =====================================================

  volatile bool receiverListNeedsRefresh = false;

  // =====================================================
  // DISCOVERY STATE
  // =====================================================

  bool discoveryActive = false;

  unsigned long discoveryStarted = 0;
  unsigned long lastDiscoverySend = 0;

  // =====================================================
  // PERIODIC COMMUNICATION
  // =====================================================

  unsigned long lastHeartbeatSend = 0;

  // Json State Callbacks
  GetJsonFromStateCallback getJsonFromStateCallback = nullptr;
  GetStateFromJsonCallback getStateFromJsonCallback = nullptr;
  ResetStateCallback resetStateCallback = nullptr;
};
extern EspnowController espnowController;

#endif  // EspnowController_H