#ifndef PACKETS_H
#define PACKETS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>
enum ESPNowCommandType : uint8_t {
  CMD_HEARTBEAT = 8,

  CMD_DISCOVER = 9,
  CMD_DISCOVERY_RESPONSE = 10,

  CMD_PAIR_REQUEST = 11,
  CMD_PAIR_RESPONSE = 12,
  CMD_UNPAIR_REQUEST = 13,
  CMD_CONTROLLER_CLAIM = 14,
  CMD_CONTROLLER_STATUS = 15,
  CMD_SERVER_STATE = 20

};
// TODO do we want to get rid of failsafe and active controller to put into data


struct ServerStatePacket {
  uint8_t type;

  JsonDocument data;
};
struct __attribute__((packed)) ESPNowCommand {
  uint8_t type;
  int16_t value;
  uint8_t data[32];
};

struct __attribute__((packed)) DiscoveryPacket {
  uint8_t type;
  int16_t value;

  uint8_t receiverMac[6];

  uint8_t pairedCount;
  uint8_t pairingMode;

  char receiverName[24];

  uint8_t channel;
};

struct __attribute__((packed)) PairPacket {
  uint8_t type;
  int16_t value;

  uint8_t receiverMac[6];

  uint8_t controllerSlot;
  uint8_t paired;

  char receiverName[24];
};

struct __attribute__((packed)) PairResponse {
  uint8_t type;
  int16_t value;

  uint8_t receiverMac[6];

  uint8_t controllerSlot;
  uint8_t paired;

  char receiverName[24];
};

#endif
