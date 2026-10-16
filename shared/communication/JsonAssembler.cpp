
#include "JsonAssembler.h"

#include <algorithm>
#include <cstring>

JsonAssembler jsonAssembler;

namespace {
JsonReassembly reassemblies[MAX_JSON_REASSEMBLIES];
uint16_t nextMessageId = 0;

// Shared scratch buffer avoids placing a 2 KB serialization buffer on the
// ESP32 task stack. This assumes disassembleJson() is called serially.
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

  // No need to clear the full 2 KB payload buffer. It is overwritten as
  // fragments arrive and is only parsed up to assembly.length.
}

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

  if (index < 0) {
    // Evict the oldest incomplete message. Signed subtraction handles
    // millis() wraparound for intervals shorter than half its range.
    index = 0;
    for (int i = 1; i < MAX_JSON_REASSEMBLIES; ++i) {
      if (static_cast<int32_t>(reassemblies[i].lastReceived -
                               reassemblies[index].lastReceived) < 0) {
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

void JsonAssembler::cleanupJsonReassemblies() {
  const unsigned long now = millis();

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; ++i) {
    JsonReassembly& assembly = reassemblies[i];

    if (assembly.active &&
        now - assembly.lastReceived > JSON_REASSEMBLY_TIMEOUT) {
      clearJsonReassembly(i);
    }
  }
}

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

  JsonFragmentHeader header;
  memcpy(&header, data, sizeof(header));

  if (header.magic != JSON_FRAGMENT_MAGIC) {
    return false;
  }

  // The packet is recognized as a fragment from here onward. Invalid or
  // incomplete fragments must not be dispatched as completed commands.
  if (len != JSON_FRAGMENT_PACKET_SIZE || header.totalChunks == 0 ||
      header.chunkIndex >= header.totalChunks ||
      header.totalChunks > MAX_CHUNKS) {
    return true;
  }

  const uint8_t* payload = data + sizeof(header);
  size_t payloadLength = len - sizeof(header);
  const bool isLastChunk = header.chunkIndex == header.totalChunks - 1;

  // JSON serialization does not contain embedded NUL bytes. The sender
  // zero-pads fixed-size packets, so only the final chunk may contain
  // trailing zero padding.
  if (isLastChunk) {
    const void* zero = memchr(payload, '\0', payloadLength);
    if (zero) {
      const size_t firstZero = static_cast<const uint8_t*>(zero) - payload;

      for (size_t i = firstZero; i < payloadLength; ++i) {
        if (payload[i] != 0) {
          return true;
        }
      }

      payloadLength = firstZero;
    }
  } else if (memchr(payload, '\0', payloadLength)) {
    return true;
  }

  if (payloadLength == 0) {
    return true;
  }

  const size_t offset =
      static_cast<size_t>(header.chunkIndex) * JSON_FRAGMENT_DATA_SIZE;

  if (offset >= JSON_REASSEMBLY_BUFFER_SIZE ||
      payloadLength >= JSON_REASSEMBLY_BUFFER_SIZE - offset) {
    return true;
  }

  const size_t end = offset + payloadLength;

  // A fixed 244-byte stride is used for every fragment except the trimmed
  // final payload. The final JSON must fit, including its terminating NUL.
  if (header.totalChunks >
      (JSON_REASSEMBLY_BUFFER_SIZE + JSON_FRAGMENT_DATA_SIZE - 1) /
          JSON_FRAGMENT_DATA_SIZE) {
    return true;
  }

  cleanupJsonReassemblies();

  const int index = allocateJsonReassembly(sourceMac);
  if (index < 0) {
    return true;
  }

  JsonReassembly& assembly = reassemblies[index];

  // A sender can only have one in-progress message in this implementation.
  // A different ID or message type starts a new assembly for that sender.
  if (assembly.receivedChunks != 0 &&
      (assembly.messageId != header.messageId ||
       assembly.totalChunks != header.totalChunks ||
       assembly.type != header.type)) {
    clearJsonReassembly(index);
    assembly.active = true;
    memcpy(assembly.sourceMac, sourceMac, sizeof(assembly.sourceMac));
  }

  assembly.messageId = header.messageId;
  assembly.totalChunks = header.totalChunks;
  assembly.type = header.type;
  assembly.lastReceived = millis();

  if (chunkReceived(assembly, header.chunkIndex)) {
    return true;
  }

  memcpy(assembly.buffer + offset, payload, payloadLength);
  markChunkReceived(assembly, header.chunkIndex);
  ++assembly.receivedChunks;

  if (end > assembly.length) {
    assembly.length = end;
  }

  if (assembly.receivedChunks != assembly.totalChunks) {
    return true;
  }

  // Every chunk is now present. The final chunk determines the true JSON
  // length, including the case where its payload exactly fills the packet.
  const size_t lastOffset =
      static_cast<size_t>(assembly.totalChunks - 1) * JSON_FRAGMENT_DATA_SIZE;

  if (assembly.length <= lastOffset ||
      assembly.length >= sizeof(assembly.buffer)) {
    clearJsonReassembly(index);
    return true;
  }

  assembly.buffer[assembly.length] = '\0';

  const DeserializationError error =
      deserializeJson(doc, assembly.buffer, assembly.length);

  if (error || !doc["type"].is<uint8_t>() ||
      doc["type"].as<uint8_t>() != assembly.type) {
    doc.clear();
    clearJsonReassembly(index);
    return true;
  }

  clearJsonReassembly(index);

  if (complete) {
    *complete = true;
  }

  return true;
}

bool JsonAssembler::disassembleJson(const uint8_t* destination,
                                    JsonDocument& doc, uint8_t*& packets,
                                    size_t& totalChunks) {
  (void)destination;

  totalChunks = 0;

  if (!packets) {
    return false;
  }

  const size_t jsonLength = measureJson(doc);

  if (jsonLength == 0 || jsonLength >= sizeof(serializationBuffer)) {
    return false;
  }

  const size_t written =
      serializeJson(doc, serializationBuffer, sizeof(serializationBuffer));

  if (written != jsonLength) {
    return false;
  }

  totalChunks =
      (written + JSON_FRAGMENT_DATA_SIZE - 1) / JSON_FRAGMENT_DATA_SIZE;

  if (totalChunks == 0 || totalChunks > MAX_CHUNKS ||
      totalChunks >
          (JSON_REASSEMBLY_BUFFER_SIZE + JSON_FRAGMENT_DATA_SIZE - 1) /
              JSON_FRAGMENT_DATA_SIZE) {
    totalChunks = 0;
    return false;
  }

  const uint8_t type =
      doc["type"].is<uint8_t>() ? doc["type"].as<uint8_t>() : 0;

  ++nextMessageId;
  if (nextMessageId == 0) {
    ++nextMessageId;
  }

  for (size_t i = 0; i < totalChunks; ++i) {
    uint8_t* packet = packets + i * JSON_FRAGMENT_PACKET_SIZE;
    memset(packet, 0, JSON_FRAGMENT_PACKET_SIZE);

    JsonFragmentHeader header{};
    header.magic = JSON_FRAGMENT_MAGIC;
    header.type = type;
    header.messageId = nextMessageId;
    header.chunkIndex = static_cast<uint8_t>(i);
    header.totalChunks = static_cast<uint8_t>(totalChunks);

    memcpy(packet, &header, sizeof(header));

    const size_t offset = i * JSON_FRAGMENT_DATA_SIZE;
    const size_t remaining = written - offset;
    const size_t chunkLength = remaining < JSON_FRAGMENT_DATA_SIZE
                                   ? remaining
                                   : JSON_FRAGMENT_DATA_SIZE;

    memcpy(packet + sizeof(header), serializationBuffer + offset, chunkLength);
  }

  return true;
}
