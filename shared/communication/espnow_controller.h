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
#include "../communication/ControllerStorage.h"
#include "../communication/packets.h"
#include "../utils/AppState.h"
#include "../utils/MacUtils.h"

// =====================================================
// ESP-NOW TIMING
// =====================================================

#define HEARTBEAT_INTERVAL 350

#define DISCOVERY_INTERVAL 1000
#define DISCOVERY_TIMEOUT 5000

// =====================================================
// RECEIVER STATE
// =====================================================

#define MAX_RECEIVERS 8

// extern unsigned long lastSpeedSend;
extern unsigned long lastHeartbeatSend;

struct ReceiverInfo {
  bool used;
  uint8_t mac[6];
  char name[24];
  uint8_t channel;
  bool paired;
  unsigned long lastSeen;
};

// =====================================================
// UI REFRESH FLAG
// =====================================================

extern volatile bool receiverListNeedsRefresh;

extern ReceiverInfo receivers[MAX_RECEIVERS];

extern int receiverCount;
extern int activeReceiver;

// =====================================================
// ESP-NOW ADDRESSES
// =====================================================

extern uint8_t broadcastMac[6];
extern uint8_t localMac[6];

// =====================================================
// DISCOVERY STATE
// =====================================================

extern bool discoveryActive;

extern unsigned long discoveryStarted;
extern unsigned long lastDiscoverySend;

// =====================================================
// ESP-NOW EVENT QUEUE
// =====================================================

enum EspNowEventType : uint8_t {
  ESP_NOW_EVENT_RECEIVE = 0,
  ESP_NOW_EVENT_SEND = 1
};

#define ESPNOW_EVENT_DATA_MAX 250
#define ESPNOW_EVENT_QUEUE_SIZE 16

struct EspNowEvent {
  EspNowEventType type;

  uint8_t mac[6];

  uint8_t data[ESPNOW_EVENT_DATA_MAX];

  uint16_t len;

  esp_now_send_status_t sendStatus;
};

extern QueueHandle_t espNowEventQueue;

// =====================================================
// EVENT QUEUE
// =====================================================
void notifyLocalStateChanged();

void queueEspNowEvent(EspNowEventType type, const uint8_t* mac,
                      const uint8_t* data, int len);

// // =====================================================
// // MAC UTILITIES
// // =====================================================

// bool macEqual(
//   const uint8_t *a,
//   const uint8_t *b
// );

// =====================================================
// RECEIVER MANAGEMENT
// =====================================================
int findReceiverByMac(const uint8_t* mac);

bool addPeer(const uint8_t* mac, uint8_t channel);

void removePeer(const uint8_t* mac);
bool removeController(int slot);
int addOrUpdateReceiver(const DiscoveryPacket* packet);

bool sendJsonDocument(JsonDocument& doc, const uint8_t* mac);
// void stopActiveReceiver();

// =====================================================
// ESP-NOW COMMANDS
// =====================================================

bool sendCommand(uint8_t type, int16_t value);
bool sendServerState(const uint8_t* destination);
// =====================================================
// DISCOVERY
// =====================================================

void sendDiscovery();

// =====================================================
// ESP-NOW CALLBACKS
// =====================================================
//
// These are intentionally NOT declared here.
// The definitions in espnow_controller.cpp below
// are registered directly with esp_now.
// =====================================================

// =====================================================
// ESP-NOW INITIALIZATION
// =====================================================

bool setupESPNow(bool wifi = false);

// =====================================================
// PERIODIC MOTOR COMMUNICATION
// =====================================================

// void sendCurrentMotorSpeed(int motorSpeed);

void sendHeartbeat();

void espnowUpdate();

void processServerState(const uint8_t* data, int len);
#endif

void processESPNowEvents();
