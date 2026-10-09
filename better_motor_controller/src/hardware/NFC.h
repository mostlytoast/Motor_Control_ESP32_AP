
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PN532.h>
#include <PN532_HSU.h>
#include <string.h>

#include <vector>

#include "../Config.h"
#include "../../shared/utils/uidLogic.h"
enum NFCReadResult {
  NFC_READ_FAILED,
  NFC_READ_DUPLICATE,
  NFC_READ_EMPTY,
  NFC_READ_SUCCESS,
  NFC_READ_MESSAGE_ERROR
};
enum NFCWriteResult {
  NFC_WRITE_FAILED,
  NFC_WRITE_DUPLICATE,
  NFC_WRITE_EMPTY,
  NFC_WRITE_SUCCESS,
  NFC_WRITE_MESSAGE_ERROR
};
class NFC {
 public:
  NFC();
  void setupNFC();

  NFCWriteResult writeNFC(JsonObject& json, const Uid& uid);
  NFCReadResult readNFC(Uid& uid, JsonObject& tagJson);

 private:
  Uid lastReadTagUid;

  bool haveLastReadTag;
  bool lastReadTagSucceeded;

  Uid lastWrittenTagUid;

  bool haveLastWrittenTag;

  bool lastWrittenTagSucceeded;

#define PN532_BAUD 115200
  String serializeMessagePack(JsonObject obj);
  bool deserializeMessagePack(const String& serialized, JsonObject& obj);
  void printHex(const uint8_t* data, uint8_t length);
  void printText(const uint8_t* data, uint8_t length);
  bool readNFCPage(uint8_t page, uint8_t* buffer);
  bool writeNFCPage(uint8_t page, const uint8_t* data);
  size_t readNFCUserMemorySize();
  bool authenticateBlock(uint8_t block, const Uid& uid);
  bool readBlock(uint8_t block, uint8_t* buffer, const Uid& uid);

  bool writeBlock(uint8_t block, const uint8_t* data, const Uid& uid);

  bool writeMifareText(const String& messagePack, const Uid& uid);

  String readMifareText(const Uid& uid);
};

extern NFC nfc;