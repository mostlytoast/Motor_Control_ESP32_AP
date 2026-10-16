#ifndef JsonAssembler_H
#define JsonAssembler_H
#include <Arduino.h>
#include <ArduinoJson.h>

#include "../utils/MacUtils.h"

struct JsonFragmentHeader {
  uint8_t magic;
  uint8_t type;
  uint16_t messageId;
  uint8_t chunkIndex;
  uint8_t totalChunks;
} __attribute__((packed));

static_assert(sizeof(JsonFragmentHeader) == 6,
              "JsonFragmentHeader must be exactly 6 bytes");

constexpr uint8_t JSON_FRAGMENT_MAGIC = 0xA5;

constexpr size_t JSON_FRAGMENT_PACKET_SIZE = 250;
constexpr size_t JSON_FRAGMENT_HEADER_SIZE = 6;

constexpr size_t JSON_FRAGMENT_DATA_SIZE =
    JSON_FRAGMENT_PACKET_SIZE - JSON_FRAGMENT_HEADER_SIZE;

constexpr size_t JSON_REASSEMBLY_BUFFER_SIZE = 8000;
constexpr unsigned long JSON_REASSEMBLY_TIMEOUT = 2000;

constexpr int MAX_JSON_REASSEMBLIES = 2;

// =====================================================
// GENERIC JSON FRAGMENTATION STATE
// =====================================================
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

// JsonReassembly jsonReassemblies[MAX_JSON_REASSEMBLIES] = {};

// uint16_t nextJsonMessageId = 0;

class JsonAssembler {
 public:
   bool processJsonFragment(const uint8_t* sourceMac, const uint8_t* data,
                           size_t len);
 private:
  bool JsonAssembler::disassembleJson(const uint8_t* destination,
                                      JsonDocument& doc);
  void cleanupJsonReassemblies();
  int findJsonReassembly(const uint8_t* sourceMac);
  void clearJsonReassembly(int index);
  void processJsonCommand(const uint8_t* sourceMac, JsonDocument& doc);
  int allocateJsonReassembly(const uint8_t* sourceMac);



};
extern JsonAssembler jsonAssembler;

#endif  // JsonAssembler_H
