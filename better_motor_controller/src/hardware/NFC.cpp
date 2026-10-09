#include "hardware/NFC.h"


// ============================================================================
// CONSTRUCTOR
// ============================================================================

NFC::NFC() {
  lastReadTagUid.clear();
  haveLastReadTag = false;

  lastWrittenTagUid.clear();
  haveLastWrittenTag = false;

  lastWrittenTagSucceeded = false;
  lastReadTagSucceeded = false;
}

// ============================================================================
// HARDWARE UART
// ============================================================================

HardwareSerial PN532Serial(2);

// ============================================================================
// PN532 HSU INTERFACE
// ============================================================================

PN532_HSU pn532hsu(PN532Serial);

// ============================================================================
// PN532
// ============================================================================

PN532 pn532(pn532hsu);

// ============================================================================
// MESSAGEPACK SERIALIZATION
// ============================================================================

String NFC::serializeMessagePack(JsonObject obj) {
  size_t size = measureMsgPack(obj);

  std::vector<uint8_t> buffer(size);

  size_t written = serializeMsgPack(obj, buffer.data(), buffer.size());

  return String((const char*)buffer.data(), written);
}

// ============================================================================
// MESSAGEPACK DESERIALIZATION
// ============================================================================

bool NFC::deserializeMessagePack(const String& serialized, JsonObject& obj) {
  JsonDocument doc;

  DeserializationError error = deserializeMsgPack(
      doc, (const uint8_t*)serialized.c_str(), serialized.length());

  if (error) {
    Serial.print("MessagePack decode failed: ");
    Serial.println(error.c_str());

    return false;
  }

  JsonObject source = doc.as<JsonObject>();

  if (source.isNull()) {
    Serial.println("MessagePack did not contain a JSON object.");
    return false;
  }

  // Copy the entire object into the caller's document
  obj.clear();
  obj.set(source);

  return true;
}

// ============================================================================
// NFC SETTINGS
// ============================================================================

constexpr uint32_t NFC_POLL_INTERVAL = 100;

uint32_t lastNFCPoll = 0;

bool nfcInitialized = false;

// ============================================================================
// MIFARE CLASSIC COMPATIBILITY
// ============================================================================

uint8_t defaultKeyA[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================================
// TYPE 2 / NTAG CONSTANTS
// ============================================================================

constexpr uint8_t NFC_CC_PAGE = 3;
constexpr uint8_t NFC_NDEF_START_PAGE = 4;

constexpr uint8_t NDEF_TLV = 0x03;
constexpr uint8_t NDEF_TERMINATOR_TLV = 0xFE;

const char NDEF_MIME_TYPE[] = "application/json";

constexpr uint8_t NDEF_MIME_TYPE_LENGTH = sizeof(NDEF_MIME_TYPE) - 1;

// ============================================================================
// PRINT HEX
// ============================================================================

void NFC::printHex(const uint8_t* data, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print('0');
    }

    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
}

// ============================================================================
// PRINT TEXT
// ============================================================================

void NFC::printText(const uint8_t* data, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    if (data[i] >= 32 && data[i] <= 126) {
      Serial.write(data[i]);
    }
  }
}

// ============================================================================
// READ TYPE 2 / NTAG PAGE
// ============================================================================

bool NFC::readNFCPage(uint8_t page, uint8_t* buffer) {
  if (buffer == nullptr) {
    return false;
  }

  if (page > 230) {
    Serial.println("NFC page out of range.");
    return false;
  }

  uint8_t result = pn532.mifareultralight_ReadPage(page, buffer);

  if (!result) {
    Serial.print("Failed to read NFC page ");
    Serial.println(page);
    return false;
  }

  return true;
}

// ============================================================================
// WRITE TYPE 2 / NTAG PAGE
// ============================================================================

bool NFC::writeNFCPage(uint8_t page, const uint8_t* data) {
  if (data == nullptr) {
    return false;
  }

  if (page > 230) {
    Serial.println("NFC page out of range.");
    return false;
  }

  uint8_t buffer[4];

  memcpy(buffer, data, 4);

  uint8_t result = pn532.mifareultralight_WritePage(page, buffer);

  if (!result) {
    Serial.print("Failed to write NFC page ");
    Serial.println(page);
    return false;
  }

  return true;
}

// ============================================================================
// GET TYPE 2 TAG USER MEMORY SIZE
// ============================================================================

size_t NFC::readNFCUserMemorySize() {
  uint8_t cc[4] = {0};

  Serial.println();
  Serial.println("Reading Capability Container...");
  Serial.println("Reading NFC page 3...");

  uint8_t result = pn532.mifareultralight_ReadPage(NFC_CC_PAGE, cc);

  Serial.print("ReadPage result: ");
  Serial.println(result);

  if (!result) {
    Serial.println("FAILED to read Capability Container.");
    return 0;
  }

  size_t memorySize = (size_t)cc[2] * 8;

  return memorySize;
}

// ============================================================================
// MIFARE CLASSIC COMPATIBILITY FUNCTION
// ============================================================================

bool NFC::authenticateBlock(uint8_t block, const Uid& uid) {
  (void)block;
  (void)uid;

  // Type 2 / NTAG tags do not use
  // MIFARE Classic Key A authentication.

  return true;
}

// ============================================================================
// LEGACY 16-BYTE READ WRAPPER
// ============================================================================

bool NFC::readBlock(uint8_t block, uint8_t* buffer, const Uid& uid) {
  (void)uid;

  if (buffer == nullptr) {
    return false;
  }

  if (block < 4) {
    Serial.println("Invalid NFC data block.");
    return false;
  }

  uint8_t firstPage = NFC_NDEF_START_PAGE + ((block - 4) * 4);

  for (uint8_t i = 0; i < 4; i++) {
    if (!readNFCPage(firstPage + i, buffer + (i * 4))) {
      Serial.print("Failed to read NFC page ");
      Serial.println(firstPage + i);

      return false;
    }
  }

  return true;
}

// ============================================================================
// LEGACY 16-BYTE WRITE WRAPPER
// ============================================================================

bool NFC::writeBlock(uint8_t block, const uint8_t* data, const Uid& uid) {
  (void)uid;

  if (data == nullptr) {
    return false;
  }

  if (block < 4) {
    Serial.println("Invalid NFC data block.");
    return false;
  }

  uint8_t firstPage = NFC_NDEF_START_PAGE + ((block - 4) * 4);

  for (uint8_t i = 0; i < 4; i++) {
    if (!writeNFCPage(firstPage + i, data + (i * 4))) {
      Serial.print("Failed to write NFC page ");
      Serial.println(firstPage + i);

      return false;
    }
  }

  return true;
}

// ============================================================================
// WRITE MESSAGEPACK
// ============================================================================

bool NFC::writeMifareText(const String& messagePack, const Uid& uid) {
  (void)uid;

  if (!nfcInitialized) {
    Serial.println("Write failed: NFC not initialized.");
    return false;
  }

  size_t messagePackLength = messagePack.length();

  if (messagePackLength == 0) {
    Serial.println("Write failed: empty MessagePack.");
    return false;
  }

  Serial.println();
  Serial.print("Writing raw MessagePack bytes: ");
  Serial.print(messagePackLength);
  Serial.println(" bytes");

  // -------------------------------------------------------------------------
  // Determine NTAG capacity
  // -------------------------------------------------------------------------

  size_t userMemory = readNFCUserMemorySize();

  if (userMemory == 0) {
    Serial.println("Write failed: could not determine tag capacity.");
    return false;
  }

  // -------------------------------------------------------------------------
  // MIME TYPE
  // -------------------------------------------------------------------------

  const char MIME_TYPE[] = "application/msgpack";

  constexpr size_t MIME_TYPE_LENGTH = sizeof(MIME_TYPE) - 1;

  // -------------------------------------------------------------------------
  // NDEF RECORD SIZE
  // -------------------------------------------------------------------------

  bool longRecord = messagePackLength > 255;

  size_t ndefRecordLength;

  if (longRecord) {
    ndefRecordLength = 1 + 1 + 4 + MIME_TYPE_LENGTH + messagePackLength;

  } else {
    ndefRecordLength = 1 + 1 + 1 + MIME_TYPE_LENGTH + messagePackLength;
  }

  // -------------------------------------------------------------------------
  // TLV SIZE
  // -------------------------------------------------------------------------

  size_t tlvLength;

  if (ndefRecordLength <= 254) {
    tlvLength = 1 + 1 + ndefRecordLength + 1;

  } else {
    tlvLength = 1 + 1 + 2 + ndefRecordLength + 1;
  }

  // -------------------------------------------------------------------------
  // CAPACITY CHECK
  // -------------------------------------------------------------------------

  if (tlvLength > userMemory) {
    Serial.println();

    Serial.println("Write failed: MessagePack is too large for this tag.");

    Serial.print("Required bytes: ");
    Serial.println(tlvLength);

    Serial.print("Available bytes: ");
    Serial.println(userMemory);

    return false;
  }

  // -------------------------------------------------------------------------
  // ALLOCATE NFC BUFFER
  // -------------------------------------------------------------------------

  uint8_t* data = (uint8_t*)malloc(tlvLength);

  if (data == nullptr) {
    Serial.println("Write failed: memory allocation failed.");

    return false;
  }

  memset(data, 0, tlvLength);

  size_t pos = 0;

  // -------------------------------------------------------------------------
  // TLV TYPE
  // -------------------------------------------------------------------------

  data[pos++] = NDEF_TLV;

  // -------------------------------------------------------------------------
  // TLV LENGTH
  // -------------------------------------------------------------------------

  if (ndefRecordLength <= 254) {
    data[pos++] = (uint8_t)ndefRecordLength;

  } else {
    data[pos++] = 0xFF;

    data[pos++] = (uint8_t)((ndefRecordLength >> 8) & 0xFF);

    data[pos++] = (uint8_t)(ndefRecordLength & 0xFF);
  }

  // -------------------------------------------------------------------------
  // NDEF HEADER
  // -------------------------------------------------------------------------

  if (longRecord) {
    data[pos++] = 0xC2;
  } else {
    data[pos++] = 0xD2;
  }

  // -------------------------------------------------------------------------
  // MIME TYPE LENGTH
  // -------------------------------------------------------------------------

  data[pos++] = (uint8_t)MIME_TYPE_LENGTH;

  // -------------------------------------------------------------------------
  // PAYLOAD LENGTH
  // -------------------------------------------------------------------------

  if (longRecord) {
    data[pos++] = (uint8_t)((messagePackLength >> 24) & 0xFF);

    data[pos++] = (uint8_t)((messagePackLength >> 16) & 0xFF);

    data[pos++] = (uint8_t)((messagePackLength >> 8) & 0xFF);

    data[pos++] = (uint8_t)(messagePackLength & 0xFF);

  } else {
    data[pos++] = (uint8_t)messagePackLength;
  }

  // -------------------------------------------------------------------------
  // MIME TYPE
  // -------------------------------------------------------------------------

  memcpy(data + pos, MIME_TYPE, MIME_TYPE_LENGTH);

  pos += MIME_TYPE_LENGTH;

  // -------------------------------------------------------------------------
  // RAW MESSAGEPACK PAYLOAD
  // -------------------------------------------------------------------------

  memcpy(data + pos, messagePack.c_str(), messagePackLength);

  pos += messagePackLength;

  // -------------------------------------------------------------------------
  // TERMINATOR
  // -------------------------------------------------------------------------

  data[pos++] = NDEF_TERMINATOR_TLV;

  // -------------------------------------------------------------------------
  // WRITE PAGES
  // -------------------------------------------------------------------------

  size_t bytesWritten = 0;

  uint8_t page = NFC_NDEF_START_PAGE;

  while (bytesWritten < tlvLength) {
    uint8_t pageData[4] = {0, 0, 0, 0};

    size_t remaining = tlvLength - bytesWritten;

    size_t bytesThisPage = min((size_t)4, remaining);

    memcpy(pageData, data + bytesWritten, bytesThisPage);

    if (!writeNFCPage(page, pageData)) {
      Serial.print("Failed writing page ");

      Serial.println(page);

      free(data);

      return false;
    }

    bytesWritten += bytesThisPage;

    page++;
  }

  free(data);

  Serial.println("Raw MessagePack successfully written to NTAG.");

  // -------------------------------------------------------------------------
  // Verify CC
  // -------------------------------------------------------------------------

  Serial.println();
  Serial.println("Testing Capability Container after write...");

  uint8_t testCC[4] = {0};

  if (readNFCPage(NFC_CC_PAGE, testCC)) {
    Serial.print("CC after write: ");

    for (uint8_t i = 0; i < 4; i++) {
      if (testCC[i] < 0x10) {
        Serial.print('0');
      }

      Serial.print(testCC[i], HEX);

      Serial.print(' ');
    }

    Serial.println();

  } else {
    Serial.println("Could NOT read page 3 after writing.");
  }

  return true;
}

// ============================================================================
// READ MESSAGEPACK
// ============================================================================

String NFC::readMifareText(const Uid& uid) {
  (void)uid;

  String result;

  if (!nfcInitialized) {
    return result;
  }

  // -------------------------------------------------------------------------
  // Get NTAG user memory size
  // -------------------------------------------------------------------------

  size_t userMemory = readNFCUserMemorySize();

  if (userMemory == 0) {
    Serial.println("Could not determine NFC memory size.");

    return result;
  }

  // -------------------------------------------------------------------------
  // Allocate memory
  // -------------------------------------------------------------------------

  uint8_t* memory = (uint8_t*)malloc(userMemory);

  if (memory == nullptr) {
    Serial.println("Failed to allocate NFC read buffer.");

    return result;
  }

  memset(memory, 0, userMemory);

  // -------------------------------------------------------------------------
  // Read all user pages
  // -------------------------------------------------------------------------

  size_t offset = 0;

  uint8_t page = NFC_NDEF_START_PAGE;

  while (offset < userMemory) {
    uint8_t pageData[4] = {0, 0, 0, 0};

    if (!readNFCPage(page, pageData)) {
      Serial.print("Failed to read page ");

      Serial.println(page);

      free(memory);

      return result;
    }

    size_t bytesToCopy = min((size_t)4, userMemory - offset);

    memcpy(memory + offset, pageData, bytesToCopy);

    offset += bytesToCopy;
    page++;
  }

  // -------------------------------------------------------------------------
  // Find NDEF TLV
  // -------------------------------------------------------------------------

  size_t pos = 0;

  while (pos < userMemory) {
    uint8_t tlvType = memory[pos++];

    // NULL TLV
    if (tlvType == 0x00) {
      continue;
    }

    // TERMINATOR
    if (tlvType == NDEF_TERMINATOR_TLV) {
      break;
    }

    // -----------------------------------------------------------------------
    // Ignore non-NDEF TLVs
    // -----------------------------------------------------------------------

    if (tlvType != NDEF_TLV) {
      if (pos >= userMemory) {
        break;
      }

      uint8_t length = memory[pos++];

      if (length == 0xFF) {
        if (pos + 2 > userMemory) {
          break;
        }

        size_t extendedLength = ((size_t)memory[pos] << 8) | memory[pos + 1];

        pos += 2;

        if (pos + extendedLength > userMemory) {
          break;
        }

        pos += extendedLength;

      } else {
        if (pos + length > userMemory) {
          break;
        }

        pos += length;
      }

      continue;
    }

    // -----------------------------------------------------------------------
    // READ NDEF LENGTH
    // -----------------------------------------------------------------------

    if (pos >= userMemory) {
      break;
    }

    size_t ndefLength = memory[pos++];

    if (ndefLength == 0xFF) {
      if (pos + 2 > userMemory) {
        break;
      }

      ndefLength = ((size_t)memory[pos] << 8) | memory[pos + 1];

      pos += 2;
    }

    if (pos + ndefLength > userMemory) {
      break;
    }

    if (ndefLength == 0) {
      break;
    }

    // -----------------------------------------------------------------------
    // RECORD START
    // -----------------------------------------------------------------------

    size_t recordStart = pos;

    // -----------------------------------------------------------------------
    // HEADER
    // -----------------------------------------------------------------------

    uint8_t header = memory[pos++];

    bool shortRecord = (header & 0x10) != 0;

    bool hasId = (header & 0x08) != 0;

    uint8_t tnf = header & 0x07;

    if (pos >= userMemory) {
      break;
    }

    // -----------------------------------------------------------------------
    // TYPE LENGTH
    // -----------------------------------------------------------------------

    uint8_t typeLength = memory[pos++];

    // -----------------------------------------------------------------------
    // PAYLOAD LENGTH
    // -----------------------------------------------------------------------

    size_t payloadLength = 0;

    if (shortRecord) {
      if (pos >= userMemory) {
        break;
      }

      payloadLength = memory[pos++];

    } else {
      if (pos + 4 > userMemory) {
        break;
      }

      payloadLength = ((size_t)memory[pos] << 24) |
                      ((size_t)memory[pos + 1] << 16) |
                      ((size_t)memory[pos + 2] << 8) | memory[pos + 3];

      pos += 4;
    }

    // -----------------------------------------------------------------------
    // ID LENGTH
    // -----------------------------------------------------------------------

    uint8_t idLength = 0;

    if (hasId) {
      if (pos >= userMemory) {
        break;
      }

      idLength = memory[pos++];
    }

    // -----------------------------------------------------------------------
    // VERIFY RECORD
    // -----------------------------------------------------------------------

    if (pos + typeLength + idLength + payloadLength >
        recordStart + ndefLength) {
      break;
    }

    // -----------------------------------------------------------------------
    // RECORD TYPE
    // -----------------------------------------------------------------------

    String recordType;

    for (uint8_t i = 0; i < typeLength; i++) {
      recordType += (char)memory[pos + i];
    }

    pos += typeLength;

    // -----------------------------------------------------------------------
    // SKIP ID
    // -----------------------------------------------------------------------

    pos += idLength;

    // -----------------------------------------------------------------------
    // MESSAGEPACK
    // -----------------------------------------------------------------------

    if (tnf == 0x02 && recordType == "application/msgpack") {
      Serial.println();

      Serial.print("Raw MessagePack read: ");

      Serial.print(payloadLength);

      Serial.println(" bytes");

      result = String((const char*)(memory + pos), payloadLength);

      free(memory);

      return result;
    }

    // -----------------------------------------------------------------------
    // NEXT RECORD
    // -----------------------------------------------------------------------

    pos = recordStart + ndefLength;
  }

  free(memory);

  Serial.println("No MessagePack NDEF record found.");

  return result;
}

// ============================================================================
// NFC SETUP
// ============================================================================

void NFC::setupNFC() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("Initializing PN532 HSU...");

  PN532Serial.begin(PN532_BAUD, SERIAL_8N1, PN532_RX, PN532_TX);

  delay(100);

  pn532.begin();

  Serial.println("Checking PN532...");

  uint32_t versiondata = pn532.getFirmwareVersion();

  if (!versiondata) {
    Serial.println();
    Serial.println("ERROR: Didn't find PN53x board!");

    return;
  }

  Serial.print("Found PN5");

  Serial.println((versiondata >> 24) & 0xFF, HEX);

  Serial.print("Firmware: ");

  Serial.print((versiondata >> 16) & 0xFF, DEC);

  Serial.print('.');

  Serial.println((versiondata >> 8) & 0xFF, DEC);

  pn532.setPassiveActivationRetries(1);

  pn532.SAMConfig();

  nfcInitialized = true;

  Serial.println();
  Serial.println("NFC ready.");
  Serial.println("Waiting for Type 2 / NTAG card...");
}

// ============================================================================
// WRITE NFC
// ============================================================================
NFCWriteResult NFC::writeNFC(JsonObject& json, const Uid& uid) {
  // -------------------------------------------------------------------------
  // DUPLICATE PROTECTION
  //
  // Only reject the write if:
  //
  //   1. We have previously written to a tag
  //   2. The UID is the same as the previous tag
  //   3. The previous write succeeded
  //
  // If the previous write failed, allow the same tag to be retried.
  // -------------------------------------------------------------------------

  if (haveLastWrittenTag && lastWrittenTagUid.equals(uid) &&
      lastWrittenTagSucceeded) {
    // Serial.println("Duplicate NFC write detected.");

    return NFC_WRITE_DUPLICATE;
  }

  // This is either:
  //
  //   - a new tag
  //   - OR the same tag whose previous write failed
  //
  // In either case, remember this UID for this write attempt.
  lastWrittenTagUid = uid;
  haveLastWrittenTag = true;

  // -------------------------------------------------------------------------
  // MEASURE MESSAGEPACK
  // -------------------------------------------------------------------------

  size_t size = measureMsgPack(json);

  if (size == 0) {
    Serial.println("writeNFC: empty MessagePack.");

    // Treat an empty MessagePack as a completed write attempt,
    // matching the behavior of the original function.
    lastWrittenTagSucceeded = true;

    return NFC_WRITE_EMPTY;
  }

  // -------------------------------------------------------------------------
  // ALLOCATE MESSAGEPACK
  // -------------------------------------------------------------------------

  uint8_t* buffer = (uint8_t*)malloc(size);

  if (buffer == nullptr) {
    Serial.println("writeNFC: MessagePack allocation failed.");

    // Write failed, so allow the same UID to be retried.
    lastWrittenTagSucceeded = false;

    return NFC_WRITE_MESSAGE_ERROR;
  }

  // -------------------------------------------------------------------------
  // SERIALIZE
  // -------------------------------------------------------------------------

  size_t written = serializeMsgPack(json, buffer, size);

  if (written == 0) {
    Serial.println("writeNFC: MessagePack serialization failed.");

    free(buffer);

    // Write failed, so allow the same UID to be retried.
    lastWrittenTagSucceeded = false;

    return NFC_WRITE_MESSAGE_ERROR;
  }

  Serial.print("MessagePack size: ");
  Serial.print(written);
  Serial.println(" bytes");

  // -------------------------------------------------------------------------
  // CONVERT BINARY MESSAGEPACK TO STRING
  // -------------------------------------------------------------------------

  String messagePack((const char*)buffer, written);

  free(buffer);

  // -------------------------------------------------------------------------
  // WRITE
  // -------------------------------------------------------------------------

  if (writeMifareText(messagePack, uid)) {
    // The write completed successfully.
    // A subsequent write to the same UID will be rejected.
    lastWrittenTagSucceeded = true;

    return NFC_WRITE_SUCCESS;
  }

  // The write failed.
  // A subsequent attempt with the same UID will be allowed.
  lastWrittenTagSucceeded = false;

  return NFC_WRITE_FAILED;
}

// ============================================================================
// READ NFC
// ============================================================================
NFCReadResult NFC::readNFC(Uid& uid, JsonObject& tagJson) {
  if (!nfcInitialized) {
    Serial.println("NFC not initialized");

    lastReadTagSucceeded = false;

    return NFC_READ_FAILED;
  }

  // -------------------------------------------------------------------------
  // TEMPORARY RAW UID BUFFER
  // -------------------------------------------------------------------------

  uint8_t rawUid[Uid::MAX_LENGTH] = {0};
  uint8_t rawUidLength = 0;

  // -------------------------------------------------------------------------
  // READ UID
  // -------------------------------------------------------------------------

  uint8_t success =
      pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, rawUid, &rawUidLength);

  if (!success) {
    lastReadTagSucceeded = false;

    return NFC_READ_FAILED;
  }

  // Convert the raw PN532 UID into our Uid object.
  uid.set(rawUid, rawUidLength);

  if (!uid.isValid()) {
    Serial.println("Invalid UID.");

    lastReadTagSucceeded = false;

    return NFC_READ_FAILED;
  }

  // -------------------------------------------------------------------------
  // DUPLICATE PROTECTION
  //
  // Only reject the tag if:
  //
  //   1. We have previously read a tag
  //   2. The UID is the same as the previous tag
  //   3. The previous read succeeded
  //
  // If the previous read failed, allow the same tag to be retried.
  // -------------------------------------------------------------------------

  if (haveLastReadTag && uid.equals(lastReadTagUid) && lastReadTagSucceeded) {
    // Serial.println("Duplicate NFC tag detected.");

    return NFC_READ_DUPLICATE;
  }

  // This is either:
  //
  //   - a new tag
  //   - OR the same tag whose previous read failed
  //
  // In either case, remember this UID for this read attempt.
  lastReadTagUid = uid;
  haveLastReadTag = true;

  // -------------------------------------------------------------------------
  // PRINT UID
  // -------------------------------------------------------------------------

  Serial.println();
  Serial.println("NFC TAG DETECTED");

  Serial.print("UID: ");
  Serial.println(uid.toString());

  Serial.print("UID length: ");
  Serial.println(uid.length());

  // -------------------------------------------------------------------------
  // READ MESSAGEPACK
  // -------------------------------------------------------------------------

  String messagePack = readMifareText(uid);

  if (messagePack.length() == 0) {
    Serial.println("No MessagePack found on tag.");

    // An empty tag is considered a successful read.
    // Therefore, the next detection of this same tag will be
    // treated as a duplicate.
    lastReadTagSucceeded = true;

    return NFC_READ_EMPTY;
  }

  Serial.print("MessagePack bytes read: ");
  Serial.println(messagePack.length());

  // -------------------------------------------------------------------------
  // DESERIALIZE
  // -------------------------------------------------------------------------

  if (deserializeMessagePack(messagePack, tagJson)) {
    Serial.println();

    // The entire read succeeded.
    // A subsequent detection of the same UID will be rejected.
    lastReadTagSucceeded = true;

    return NFC_READ_SUCCESS;
  }

  // The tag was detected, but reading/deserializing its data failed.
  //
  // Keep lastReadTagUid as this UID, but mark the read as failed so that
  // the same physical tag can be retried on the next scan.
  lastReadTagSucceeded = false;

  return NFC_READ_MESSAGE_ERROR;
}
