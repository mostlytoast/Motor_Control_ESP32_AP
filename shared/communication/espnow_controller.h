#ifndef ESPNOW_CONTROLLER_H
#define ESPNOW_CONTROLLER_H

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
#include "ControllerStorage.h"
#include "StateJson.h"
#include "packets.h"

// =====================================================
// ESP-NOW TIMING
// =====================================================

#define HEARTBEAT_INTERVAL 350
#define DISCOVERY_INTERVAL 1000
#define DISCOVERY_TIMEOUT 5000

// =====================================================
// GENERIC JSON FRAGMENTATION
// =====================================================

constexpr uint8_t JSON_FRAGMENT_MAGIC = 0xA5;

constexpr size_t JSON_FRAGMENT_PACKET_SIZE = 250;
constexpr size_t JSON_FRAGMENT_HEADER_SIZE = 6;

constexpr size_t JSON_FRAGMENT_DATA_SIZE =
    JSON_FRAGMENT_PACKET_SIZE - JSON_FRAGMENT_HEADER_SIZE;

constexpr size_t JSON_REASSEMBLY_BUFFER_SIZE = 8000;
constexpr unsigned long JSON_REASSEMBLY_TIMEOUT = 2000;

constexpr int MAX_JSON_REASSEMBLIES = 2;

struct JsonFragmentHeader {
  uint8_t magic;
  uint8_t type;
  uint16_t messageId;
  uint8_t chunkIndex;
  uint8_t totalChunks;
} __attribute__((packed));

static_assert(sizeof(JsonFragmentHeader) == 6,
              "JsonFragmentHeader must be exactly 6 bytes");

struct JsonReassembly {
  bool active = false;
  uint8_t sourceMac[6] = {};

  uint8_t type = 0;
  uint16_t messageId = 0;
  uint8_t totalChunks = 0;
  uint8_t receivedChunks = 0;

  size_t length = 0;

  bool received[255] = {};
  char buffer[JSON_REASSEMBLY_BUFFER_SIZE];

  unsigned long lastReceived = 0;
};

// =====================================================
// RECEIVER STATE
// =====================================================

#define MAX_RECEIVERS 8

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
// STATE SYNCHRONIZATION
// =====================================================

enum StateChangeOrigin {
  STATE_CHANGE_NONE,
  STATE_CHANGE_LOCAL,
  STATE_CHANGE_REMOTE
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
  uint8_t mac[6];
  uint8_t data[ESPNOW_EVENT_DATA_MAX];
  uint16_t len;
  esp_now_send_status_t sendStatus;
};

// =====================================================
// GENERIC JSON FRAGMENTATION STATE
// =====================================================

inline JsonReassembly jsonReassemblies[MAX_JSON_REASSEMBLIES] = {};

inline uint16_t nextJsonMessageId = 0;

// =====================================================
// LOCAL ESP-NOW STATE
// =====================================================

inline uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

inline uint8_t localMac[6] = {};

inline QueueHandle_t espNowEventQueue = nullptr;

// =====================================================
// CONTROLLER-SIDE RECEIVER LIST
// =====================================================

inline int receiverCount = 0;
inline int activeReceiver = -1;

inline volatile bool receiverListNeedsRefresh = false;

inline ReceiverInfo receivers[MAX_RECEIVERS] = {};

// =====================================================
// DISCOVERY STATE
// =====================================================

inline bool discoveryActive = false;

inline unsigned long discoveryStarted = 0;
inline unsigned long lastDiscoverySend = 0;

// =====================================================
// CONTROLLER-SIDE SERVER STATE
// =====================================================

inline JsonDocument serverState;

// =====================================================
// PERIODIC COMMUNICATION
// =====================================================

inline unsigned long lastHeartbeatSend = 0;

// =====================================================
// STATE SYNCHRONIZATION STATE
// =====================================================

inline volatile StateChangeOrigin stateChangeOrigin = STATE_CHANGE_NONE;

inline volatile bool applyingRemoteState = false;

// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

// Event queue
void notifyLocalStateChanged();

void queueEspNowEvent(EspNowEventType type, const uint8_t* mac,
                      const uint8_t* data, int len);

// Receiver management
int findReceiverByMac(const uint8_t* mac);

bool addPeer(const uint8_t* mac, uint8_t channel);

void removePeer(const uint8_t* mac);

bool removeController(int slot);

int addOrUpdateReceiver(const DiscoveryPacket* packet);

// JSON communication
bool sendJsonDocument(JsonDocument& doc, const uint8_t* mac);

// ESP-NOW commands
bool sendCommand(uint8_t type, int16_t value);

bool sendServerState(const uint8_t* destination);

// Discovery
void sendDiscovery();

// Initialization
bool setupESPNow(bool wifi = false);

// Periodic communication
void sendHeartbeat();

void espnowUpdate();

// Incoming state
void processServerState(const uint8_t* data, int len);

void processESPNowEvents();

// Pairing
sendPairRequestState sendPairRequest(int index);

validateSelectedPeerState validateSelectedPeer(int index);

// Discovery management
void resetDiscover();
bool hasActiveReceiver();
bool isActiveReceiver(int i);

#endif  // ESPNOW_CONTROLLER_H
