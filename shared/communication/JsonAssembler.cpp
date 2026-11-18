#include "JsonAssembler.h"

#include <stdint.h>

#include <algorithm>
#include <cstring>

JsonAssembler jsonAssembler;

namespace {

JsonReassembly reassemblies[MAX_JSON_REASSEMBLIES];

uint16_t nextMessageId = 0;

// Shared scratch buffer avoids putting a large serialization buffer
// on the ESP32 task stack.
//
// IMPORTANT: disassembleJson() must be called serially.
char serializationBuffer[JSON_REASSEMBLY_BUFFER_SIZE];

bool sameMac(const uint8_t* a, const uint8_t* b) {
  return a && b && memcmp(a, b, 6) == 0;
}

bool chunkReceived(const JsonReassembly& assembly, uint8_t index) {
  return (assembly.received[index / 8] &
          static_cast<uint8_t>(1U << (index % 8))) != 0;
}

void markChunkReceived(JsonReassembly& assembly, uint8_t index) {
  assembly.received[index / 8] |= static_cast<uint8_t>(1U << (index % 8));
}

}  // namespace

// ------------------------------------------------------------
// Find an assembly belonging to a sender.
// This legacy function returns the first assembly for that MAC.
// ------------------------------------------------------------

int JsonAssembler::findJsonReassembly(const uint8_t* sourceMac) {
  if (!sourceMac) {
    return -1;
  }

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; ++i) {
    if (reassemblies[i].active &&
        sameMac(reassemblies[i].sourceMac, sourceMac)) {
      return i;
    }
  }

  return -1;
}

// ------------------------------------------------------------
// Clear an assembly.
// The payload buffer does not need to be cleared because its
// contents are only used after every chunk has been received.
// ------------------------------------------------------------

void JsonAssembler::clearJsonReassembly(int index) {
  if (index < 0 || index >= MAX_JSON_REASSEMBLIES) {
    return;
  }

  JsonReassembly& assembly = reassemblies[index];

  assembly.active = false;

  memset(assembly.sourceMac, 0, sizeof(assembly.sourceMac));

  assembly.type = 0;
  assembly.messageId = 0;
  assembly.totalChunks = 0;
  assembly.receivedChunks = 0;
  assembly.length = 0;

  memset(assembly.received, 0, sizeof(assembly.received));

  assembly.lastReceived = 0;
}

// ------------------------------------------------------------
// Allocate an assembly for a sender.
// Retained for compatibility with the existing class interface.
// ------------------------------------------------------------

int JsonAssembler::allocateJsonReassembly(const uint8_t* sourceMac) {
  if (!sourceMac) {
    return -1;
  }

  int index = findJsonReassembly(sourceMac);

  if (index >= 0) {
    return index;
  }

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; ++i) {
    if (!reassemblies[i].active) {
      index = i;
      break;
    }
  }

  // If no free slots exist, evict the oldest assembly.
  if (index < 0) {
    const unsigned long now = millis();

    index = 0;
    unsigned long oldestAge = 0;

    for (int i = 0; i < MAX_JSON_REASSEMBLIES; ++i) {
      const unsigned long age = now - reassemblies[i].lastReceived;

      if (age >= oldestAge) {
        oldestAge = age;
        index = i;
      }
    }
  }

  clearJsonReassembly(index);

  JsonReassembly& assembly = reassemblies[index];

  assembly.active = true;

  memcpy(assembly.sourceMac, sourceMac, sizeof(assembly.sourceMac));

  assembly.lastReceived = millis();

  return index;
}

// ------------------------------------------------------------
// Remove assemblies that have timed out.
// ------------------------------------------------------------

void JsonAssembler::cleanupJsonReassemblies() {
  const unsigned long now = millis();

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; ++i) {
    JsonReassembly& assembly = reassemblies[i];

    if (!assembly.active) {
      continue;
    }

    if (now - assembly.lastReceived > JSON_REASSEMBLY_TIMEOUT) {
      clearJsonReassembly(i);
    }
  }
}

// ------------------------------------------------------------
// Receive and reassemble a JSON fragment.
//
// Return values:
//
// false:
//   Not a recognized JSON fragment.
//
// true + complete == false:
//   Recognized fragment, but message is incomplete or invalid.
//
// true + complete == true:
//   Successfully reconstructed JSON in doc.
// ------------------------------------------------------------

bool JsonAssembler::jsonFragmentToJson(const uint8_t* sourceMac,
                                       const uint8_t* data, size_t len,
                                       JsonDocument& doc, bool* complete) {
  if (complete) {
    *complete = false;
  }

  doc.clear();

  if (!sourceMac || !data || len < sizeof(JsonFragmentHeader)) {
    return false;
  }

  // ----------------------------------------------------------
  // Read and validate header.
  // ----------------------------------------------------------

  JsonFragmentHeader header;

  memcpy(&header, data, sizeof(header));

  if (header.magic != JSON_FRAGMENT_MAGIC) {
    return false;
  }

  // From this point onward, this is a recognized fragment.
  // Invalid fragments must not be dispatched as normal commands.

  if (len != JSON_FRAGMENT_PACKET_SIZE || header.totalChunks == 0 ||
      header.totalChunks > MAX_CHUNKS ||
      header.chunkIndex >= header.totalChunks) {
    return true;
  }

  if (JSON_FRAGMENT_PACKET_SIZE !=
      sizeof(JsonFragmentHeader) + JSON_FRAGMENT_DATA_SIZE) {
    return true;
  }

  const uint8_t* payload = data + sizeof(JsonFragmentHeader);

  size_t payloadLength = len - sizeof(JsonFragmentHeader);

  const bool isLastChunk = header.chunkIndex == header.totalChunks - 1;

  // ----------------------------------------------------------
  // Validate fragment contents.
  //
  // Every non-final fragment must contain exactly the expected
  // payload size.
  //
  // Only the final fragment can contain zero padding.
  // ----------------------------------------------------------

  if (!isLastChunk) {
    if (payloadLength != JSON_FRAGMENT_DATA_SIZE) {
      return true;
    }

    // JSON serialization cannot contain literal NUL bytes.
    if (memchr(payload, '\0', payloadLength)) {
      return true;
    }

  } else {
    // Find the first padding byte.
    const void* zero = memchr(payload, '\0', payloadLength);

    if (zero) {
      const size_t firstZero = static_cast<const uint8_t*>(zero) - payload;

      // Everything after the first padding byte must also be zero.
      for (size_t i = firstZero; i < payloadLength; ++i) {
        if (payload[i] != 0) {
          return true;
        }
      }

      payloadLength = firstZero;
    }
  }

  if (payloadLength == 0) {
    return true;
  }

  // ----------------------------------------------------------
  // Calculate fragment position.
  // ----------------------------------------------------------

  const size_t offset =
      static_cast<size_t>(header.chunkIndex) * JSON_FRAGMENT_DATA_SIZE;

  const size_t bufferCapacity = sizeof(reassemblies[0].buffer);

  // Leave one byte for the terminating NUL.
  if (bufferCapacity < 2 || offset >= bufferCapacity ||
      payloadLength > bufferCapacity - offset - 1) {
    return true;
  }

  // The last fragment must begin before the final available
  // buffer byte, otherwise even a one-byte payload cannot fit.
  const size_t lastOffset =
      static_cast<size_t>(header.totalChunks - 1) * JSON_FRAGMENT_DATA_SIZE;

  if (lastOffset >= bufferCapacity - 1) {
    return true;
  }

  const size_t end = offset + payloadLength;

  // ----------------------------------------------------------
  // Clean up timed-out messages.
  // ----------------------------------------------------------

  cleanupJsonReassemblies();

  // ----------------------------------------------------------
  // Find an assembly using BOTH source MAC and message ID.
  //
  // This allows a sender to have multiple messages in progress.
  // It also prevents fragments from different message IDs from
  // being combined into the same JSON document.
  // ----------------------------------------------------------

  int index = -1;

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; ++i) {
    const JsonReassembly& assembly = reassemblies[i];

    if (assembly.active && sameMac(assembly.sourceMac, sourceMac) &&
        assembly.messageId == header.messageId) {
      index = i;
      break;
    }
  }

  // ----------------------------------------------------------
  // No matching message exists. Allocate a new assembly.
  // ----------------------------------------------------------

  if (index < 0) {
    // First look for an unused slot.
    for (int i = 0; i < MAX_JSON_REASSEMBLIES; ++i) {
      if (!reassemblies[i].active) {
        index = i;
        break;
      }
    }

    // If every slot is occupied, evict the oldest assembly.
    if (index < 0) {
      const unsigned long now = millis();

      unsigned long oldestAge = 0;

      index = 0;

      for (int i = 0; i < MAX_JSON_REASSEMBLIES; ++i) {
        const unsigned long age = now - reassemblies[i].lastReceived;

        if (age >= oldestAge) {
          oldestAge = age;
          index = i;
        }
      }
    }

    clearJsonReassembly(index);

    JsonReassembly& assembly = reassemblies[index];

    assembly.active = true;

    memcpy(assembly.sourceMac, sourceMac, sizeof(assembly.sourceMac));

    assembly.messageId = header.messageId;
    assembly.totalChunks = header.totalChunks;
    assembly.type = header.type;
  }

  JsonReassembly& assembly = reassemblies[index];

  // ----------------------------------------------------------
  // Validate existing assembly metadata.
  //
  // A message ID should uniquely identify one message from a
  // sender while that message is active.
  // ----------------------------------------------------------

  if (assembly.messageId != header.messageId ||
      assembly.totalChunks != header.totalChunks ||
      assembly.type != header.type) {
    clearJsonReassembly(index);

    assembly.active = true;

    memcpy(assembly.sourceMac, sourceMac, sizeof(assembly.sourceMac));

    assembly.messageId = header.messageId;
    assembly.totalChunks = header.totalChunks;
    assembly.type = header.type;
  }

  assembly.lastReceived = millis();

  // ----------------------------------------------------------
  // Handle duplicate fragments.
  //
  // Identical retransmissions are ignored.
  // Conflicting retransmissions invalidate the assembly.
  // ----------------------------------------------------------

  if (chunkReceived(assembly, header.chunkIndex)) {
    size_t existingLength = JSON_FRAGMENT_DATA_SIZE;

    if (isLastChunk) {
      if (assembly.length <= offset) {
        clearJsonReassembly(index);
        return true;
      }

      existingLength = assembly.length - offset;
    }

    if (existingLength != payloadLength ||
        memcmp(assembly.buffer + offset, payload, payloadLength) != 0) {
      clearJsonReassembly(index);
    }

    return true;
  }

  // ----------------------------------------------------------
  // Store fragment.
  // ----------------------------------------------------------

  memcpy(assembly.buffer + offset, payload, payloadLength);

  markChunkReceived(assembly, header.chunkIndex);

  ++assembly.receivedChunks;

  // The highest fragment offset determines the current endpoint.
  // Since all non-final fragments have fixed lengths, and the
  // final fragment has the highest offset, this also correctly
  // handles fragments arriving out of order.
  if (end > assembly.length) {
    assembly.length = end;
  }

  // ----------------------------------------------------------
  // Check whether all fragments have arrived.
  // ----------------------------------------------------------

  if (assembly.receivedChunks != assembly.totalChunks) {
    return true;
  }

  // ----------------------------------------------------------
  // Validate final JSON length.
  // ----------------------------------------------------------

  if (assembly.length <= lastOffset || assembly.length >= bufferCapacity) {
    clearJsonReassembly(index);
    return true;
  }

  // Add terminating NUL.
  assembly.buffer[assembly.length] = '\0';

  // ----------------------------------------------------------
  // Reconstruct JSON.
  // ----------------------------------------------------------

  const DeserializationError error =
      deserializeJson(doc, assembly.buffer, assembly.length);

  // ----------------------------------------------------------
  // Validate reconstructed document.
  // ----------------------------------------------------------

  if (error || !doc["type"].is<uint8_t>() ||
      doc["type"].as<uint8_t>() != assembly.type) {
    doc.clear();

    clearJsonReassembly(index);

    return true;
  }

  // ----------------------------------------------------------
  // Successfully reconstructed message.
  // ----------------------------------------------------------

  clearJsonReassembly(index);

  if (complete) {
    *complete = true;
  }

  return true;
}

// ------------------------------------------------------------
// Convert JSON into fixed-size ESP-NOW packets.
//
// IMPORTANT:
// The caller must allocate enough memory for:
// totalChunks * JSON_FRAGMENT_PACKET_SIZE
//
// The existing function signature does not provide a buffer
// capacity, so this function cannot independently verify the
// caller's allocation size.
// ------------------------------------------------------------

bool JsonAssembler::disassembleJson(JsonDocument& doc, uint8_t*& packets,
                                    size_t& totalChunks) {
  // Serial.println("[JsonAssembler] Chunking JSON");

  totalChunks = 0;

  if (!packets) {
    Serial.println("[JsonAssembler] Invalid packet buffer");
    return false;
  }

  // ----------------------------------------------------------
  // Validate message type.
  // ----------------------------------------------------------

  if (!doc["type"].is<uint8_t>()) {
    Serial.println("[JsonAssembler] Missing or invalid JSON type");
    return false;
  }

  const uint8_t type = doc["type"].as<uint8_t>();

  // ----------------------------------------------------------
  // Measure JSON.
  // ----------------------------------------------------------

  const size_t jsonLength = measureJson(doc);

  if (jsonLength == 0 || jsonLength >= sizeof(serializationBuffer)) {
    Serial.println("[JsonAssembler] JSON too large or empty");
    return false;
  }

  // ----------------------------------------------------------
  // Serialize JSON into shared scratch buffer.
  // ----------------------------------------------------------

  const size_t written =
      serializeJson(doc, serializationBuffer, sizeof(serializationBuffer));

  if (written != jsonLength) {
    Serial.println("[JsonAssembler] JSON serialization failed");
    return false;
  }

  // ----------------------------------------------------------
  // Calculate number of chunks.
  // ----------------------------------------------------------

  totalChunks =
      (written + JSON_FRAGMENT_DATA_SIZE - 1) / JSON_FRAGMENT_DATA_SIZE;

  if (totalChunks == 0 || totalChunks > MAX_CHUNKS || totalChunks > UINT8_MAX) {
    totalChunks = 0;

    Serial.println("[JsonAssembler] Too many chunks");
    return false;
  }

  if (totalChunks >
      (JSON_REASSEMBLY_BUFFER_SIZE + JSON_FRAGMENT_DATA_SIZE - 2) /
          JSON_FRAGMENT_DATA_SIZE) {
    totalChunks = 0;

    Serial.println("[JsonAssembler] Exceeds reassembly capacity");
    return false;
  }

  // ----------------------------------------------------------
  // Generate unique message ID.
  // ----------------------------------------------------------

  ++nextMessageId;

  // Reserve zero as an unused message ID.
  if (nextMessageId == 0) {
    ++nextMessageId;
  }

  // ----------------------------------------------------------
  // Generate packets.
  // ----------------------------------------------------------

  for (size_t i = 0; i < totalChunks; ++i) {
    uint8_t* packet = packets + i * JSON_FRAGMENT_PACKET_SIZE;

    // Clear the entire packet, including final-chunk padding.
    memset(packet, 0, JSON_FRAGMENT_PACKET_SIZE);

    // Construct fragment header.
    JsonFragmentHeader header{};

    header.magic = JSON_FRAGMENT_MAGIC;
    header.type = type;
    header.messageId = nextMessageId;
    header.chunkIndex = static_cast<uint8_t>(i);
    header.totalChunks = static_cast<uint8_t>(totalChunks);

    memcpy(packet, &header, sizeof(header));

    // Calculate data location.
    const size_t offset = i * JSON_FRAGMENT_DATA_SIZE;

    const size_t remaining = written - offset;

    const size_t chunkLength =
        std::min(remaining, static_cast<size_t>(JSON_FRAGMENT_DATA_SIZE));

    // Copy JSON data into packet.
    memcpy(packet + sizeof(header), serializationBuffer + offset, chunkLength);
  }

  // Serial.print("[JsonAssembler] JSON length: ");
  // Serial.println(written);

  // Serial.print("[JsonAssembler] Message ID: ");
  // Serial.println(nextMessageId);

  // Serial.print("[JsonAssembler] Total chunks: ");
  // Serial.println(totalChunks);

  return true;
}