#include "JsonAssembler.h"
// =====================================================
// JSON REASSEMBLY HELPERS
// =====================================================
JsonAssembler jsonAssembler;

namespace {
JsonReassembly jsonReassemblies[MAX_CONTROLLERS];
uint16_t nextJsonMessageId = 0;

}  // namespace
int JsonAssembler::findJsonReassembly(const uint8_t* sourceMac) {
  if (sourceMac == nullptr) {
    return -1;
  }

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; i++) {
    if (!jsonReassemblies[i].active) {
      continue;
    }

    if (macEqual(jsonReassemblies[i].sourceMac, sourceMac)) {
      return i;
    }
  }

  return -1;
}

void JsonAssembler::clearJsonReassembly(int index) {
  if (index < 0 || index >= MAX_JSON_REASSEMBLIES) {
    return;
  }

  memset(&jsonReassemblies[index], 0, sizeof(JsonReassembly));
}

int JsonAssembler::allocateJsonReassembly(const uint8_t* sourceMac) {
  // ---------------------------------------------------
  // First try to find an existing one
  // ---------------------------------------------------

  int existing = findJsonReassembly(sourceMac);

  if (existing >= 0) {
    return existing;
  }

  // ---------------------------------------------------
  // Find free slot
  // ---------------------------------------------------

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; i++) {
    if (!jsonReassemblies[i].active) {
      clearJsonReassembly(i);

      jsonReassemblies[i].active = true;

      memcpy(jsonReassemblies[i].sourceMac, sourceMac, 6);

      return i;
    }
  }

  // ---------------------------------------------------
  // No free slot.
  //
  // Reuse the oldest one.
  // ---------------------------------------------------

  int oldest = 0;

  for (int i = 1; i < MAX_JSON_REASSEMBLIES; i++) {
    if (jsonReassemblies[i].lastReceived <
        jsonReassemblies[oldest].lastReceived) {
      oldest = i;
    }
  }

  clearJsonReassembly(oldest);

  jsonReassemblies[oldest].active = true;

  memcpy(jsonReassemblies[oldest].sourceMac, sourceMac, 6);

  return oldest;
}

void JsonAssembler::cleanupJsonReassemblies() {
  const unsigned long now = millis();

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; i++) {
    if (!jsonReassemblies[i].active) {
      continue;
    }

    if (now - jsonReassemblies[i].lastReceived > JSON_REASSEMBLY_TIMEOUT) {
      Serial.print("JSON reassembly timeout from ");

      Serial.println(macToString(jsonReassemblies[i].sourceMac));

      clearJsonReassembly(i);
    }
  }
}

// =====================================================
// PROCESS JSON FRAGMENT
//
// Returns true if the packet was recognized as a JSON
// fragment.
//
// When all fragments have arrived, the reconstructed
// JSON is passed to the appropriate JSON handler.
// =====================================================

bool JsonAssembler::processJsonFragment(const uint8_t* sourceMac,
                                        const uint8_t* data, size_t len) {
  if (sourceMac == nullptr || data == nullptr) {
    return false;
  }

  // ---------------------------------------------------
  // Not enough bytes for a header
  // ---------------------------------------------------

  if (len < sizeof(JsonFragmentHeader)) {
    return false;
  }

  JsonFragmentHeader header;

  memcpy(&header, data, sizeof(header));

  // ---------------------------------------------------
  // Not a JSON fragment
  // ---------------------------------------------------

  if (header.magic != JSON_FRAGMENT_MAGIC) {
    return false;
  }

  // ---------------------------------------------------
  // Validate header
  // ---------------------------------------------------

  if (header.totalChunks == 0) {
    Serial.println("Invalid JSON fragment count");

    return true;
  }

  if (header.chunkIndex >= header.totalChunks) {
    Serial.println("Invalid JSON fragment index");

    return true;
  }

  size_t chunkLength = len - sizeof(JsonFragmentHeader);

  if (chunkLength == 0) {
    Serial.println("Empty JSON fragment");

    return true;
  }

  // ---------------------------------------------------
  // Get reassembly slot for this sender
  // ---------------------------------------------------

  int slot = findJsonReassembly(sourceMac);

  // ---------------------------------------------------
  // New message
  // ---------------------------------------------------

  if (slot < 0) {
    slot = allocateJsonReassembly(sourceMac);

    JsonReassembly& assembly = jsonReassemblies[slot];

    assembly.type = header.type;

    assembly.messageId = header.messageId;

    assembly.totalChunks = header.totalChunks;

    assembly.receivedChunks = 0;

    assembly.length = 0;

    memset(assembly.received, 0, sizeof(assembly.received));
  }

  JsonReassembly& assembly = jsonReassemblies[slot];

  // ---------------------------------------------------
  // If this is a different message from the same
  // sender, start over.
  //
  // This also handles a new message arriving before
  // the previous one has completed.
  // ---------------------------------------------------

  if (assembly.messageId != header.messageId ||
      assembly.totalChunks != header.totalChunks ||
      assembly.type != header.type) {
    clearJsonReassembly(slot);

    assembly.active = true;

    memcpy(assembly.sourceMac, sourceMac, 6);

    assembly.type = header.type;

    assembly.messageId = header.messageId;

    assembly.totalChunks = header.totalChunks;

    assembly.receivedChunks = 0;

    assembly.length = 0;

    memset(assembly.received, 0, sizeof(assembly.received));
  }

  assembly.lastReceived = millis();

  // ---------------------------------------------------
  // Ignore duplicate fragment
  // ---------------------------------------------------

  if (assembly.received[header.chunkIndex]) {
    Serial.print("Duplicate JSON chunk ");

    Serial.println(header.chunkIndex);

    return true;
  }

  // ---------------------------------------------------
  // Calculate where this chunk belongs
  // ---------------------------------------------------

  size_t offset =
      static_cast<size_t>(header.chunkIndex) * JSON_FRAGMENT_DATA_SIZE;

  // ---------------------------------------------------
  // Check buffer
  // ---------------------------------------------------

  if (offset + chunkLength >= JSON_REASSEMBLY_BUFFER_SIZE) {
    Serial.println("JSON reassembly buffer overflow");

    clearJsonReassembly(slot);

    return true;
  }

  // ---------------------------------------------------
  // Copy chunk
  // ---------------------------------------------------

  memcpy(assembly.buffer + offset, data + sizeof(JsonFragmentHeader),
         chunkLength);

  assembly.received[header.chunkIndex] = true;

  assembly.receivedChunks++;

  size_t end = offset + chunkLength;

  if (end > assembly.length) {
    assembly.length = end;
  }
  // ---------------------------------------------------
  // Still waiting for chunks
  // ---------------------------------------------------

  if (assembly.receivedChunks < assembly.totalChunks) {
    return true;
  }

  // ---------------------------------------------------
  // COMPLETE
  // ---------------------------------------------------

  assembly.buffer[assembly.length] = '\0';

  // ---------------------------------------------------
  // Parse reconstructed JSON
  // ---------------------------------------------------

  JsonDocument doc;

  DeserializationError error =
      deserializeJson(doc, assembly.buffer, assembly.length);

  if (error) {
    Serial.print("Reassembled JSON parse failed: ");

    Serial.println(error.c_str());

    clearJsonReassembly(slot);

    return true;
  }

  // ---------------------------------------------------
  // Validate type
  // ---------------------------------------------------

  if (!doc["type"].is<uint8_t>()) {
    Serial.println("Reassembled JSON has no type");

    clearJsonReassembly(slot);

    return true;
  }

  uint8_t type = doc["type"].as<uint8_t>();

  // ---------------------------------------------------
  // SERVER STATE
  // ---------------------------------------------------

  if (type == CMD_SERVER_STATE) {
    int receiverIndex = findController(sourceMac);

    if (receiverIndex >= 0) {
      receivers[receiverIndex].lastSeen = millis();
    }

    processServerState(reinterpret_cast<const uint8_t*>(assembly.buffer),
                       assembly.length);
  }

  // ---------------------------------------------------
  // Other JSON commands
  // ---------------------------------------------------

  else {
    processJsonCommand(sourceMac, doc);
  }

  // ---------------------------------------------------
  // Done
  // ---------------------------------------------------

  clearJsonReassembly(slot);

  return true;
}

bool JsonAssembler::disassembleJson(const uint8_t* destination,
                                    JsonDocument& doc, uint8_t* packets) {
  String json;

  serializeJson(doc, json);

  if (json.length() == 0) {
    Serial.println("sendJsonDocument: empty JSON");

    return false;
  }

  // ---------------------------------------------------
  // Calculate number of fragments
  // ---------------------------------------------------

  size_t totalChunks =
      (json.length() + JSON_FRAGMENT_DATA_SIZE - 1) / JSON_FRAGMENT_DATA_SIZE;

  if (totalChunks == 0) {
    return false;
  }

  if (totalChunks > 255) {
    Serial.print("JSON too large: ");

    Serial.print(json.length());

    Serial.println(" bytes. Maximum is 255 chunks.");

    return false;
  }

  // ---------------------------------------------------
  // Message ID
  // ---------------------------------------------------

  uint16_t messageId = ++nextJsonMessageId;

  if (messageId == 0) {
    messageId = ++nextJsonMessageId;
  }

  // ---------------------------------------------------
  // Determine JSON type
  // ---------------------------------------------------

  uint8_t type = 0;

  if (doc["type"].is<uint8_t>()) {
    type = doc["type"].as<uint8_t>();
  }

  // ---------------------------------------------------
  // Send every fragment
  // ---------------------------------------------------

  for (size_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    uint8_t packet[JSON_FRAGMENT_PACKET_SIZE];

    JsonFragmentHeader header = {};

    header.magic = JSON_FRAGMENT_MAGIC;

    header.type = type;

    header.messageId = messageId;

    header.chunkIndex = static_cast<uint8_t>(chunkIndex);

    header.totalChunks = static_cast<uint8_t>(totalChunks);

    memcpy(packet, &header, sizeof(header));

    // -------------------------------------------------
    // Calculate chunk data
    // -------------------------------------------------

    size_t offset = chunkIndex * JSON_FRAGMENT_DATA_SIZE;

    size_t remaining = json.length() - offset;

    size_t chunkLength = min(remaining, JSON_FRAGMENT_DATA_SIZE);

    memcpy(packet + sizeof(header), json.c_str() + offset, chunkLength);

    size_t packetLength = sizeof(header) + chunkLength;

    // -------------------------------------------------
    // Send
    // -------------------------------------------------

    // combine into single array of packets 
  }
}