#pragma once

#include <Arduino.h>
// #include <Arduino.h> cant include as this script gets imported by map which
// is used in webassm
#include <stdint.h>
#include <string.h>

class Uid {

 
 public:
 static constexpr uint8_t MAX_LENGTH = 7;
  Uid(const char* uidString) { fromString(uidString); };

  Uid(const String& uidString) : Uid() { fromString(uidString); };

  Uid() { clear(); };

  Uid(const uint8_t* uid, uint8_t length) {
    clear();
    set(uid, length);
  };
  static uint8_t getMaxLength() { return MAX_LENGTH; };
  void random() {
    while (uidLength < MAX_LENGTH) {
      int randomNum = rand() % 255;
      uid[uidLength] = (uint8_t)randomNum;
      uidLength++;
    }
  }
  // ============================================================
  // SET UID
  // ============================================================

  void set(const uint8_t* source, uint8_t length) {
    clear();

    if (source == nullptr) {
      return;
    }

    if (length > MAX_LENGTH) {
      length = MAX_LENGTH;
    }

    memcpy(uid, source, length);
    uidLength = length;
  };

  // ============================================================
  // STRING TO UID
  // ============================================================

  // ============================================================
  // GETTERS
  // ============================================================

  const uint8_t* data() const { return uid; };

  uint8_t length() const { return uidLength; };

  // ============================================================
  // UID TO STRING
  // ============================================================

  String toString() const {
    String result;

    for (uint8_t i = 0; i < uidLength; i++) {
      if (i > 0) {
        result += ":";
      }

      if (uid[i] < 0x10) {
        result += "0";
      }

      result += String(uid[i], HEX);
    }

    result.toUpperCase();

    return result;
  };

  bool fromString(const char* uidString) {
    clear();

    if (uidString == nullptr || uidString[0] == '\0') {
      return false;
    }

    const char* p = uidString;

    while (*p != '\0' && uidLength < MAX_LENGTH) {
      // Skip colon separators
      if (*p == ':') {
        p++;
        continue;
      }

      // Need two hex characters
      if (p[0] == '\0' || p[1] == '\0') {
        return false;
      }

      char byteString[3];

      byteString[0] = p[0];
      byteString[1] = p[1];
      byteString[2] = '\0';

      char* endPtr = nullptr;

      unsigned long value = strtoul(byteString, &endPtr, 16);

      // Invalid hexadecimal value
      if (endPtr != byteString || endPtr == byteString + 2) {
        uid[uidLength] = (uint8_t)value;
        uidLength++;
      } else {
        // Serial.print("fromString: Invalid UID: ");
        // Serial.println(uidString);
        clear();
        return false;
      }

      p += 2;

      // If the next character isn't a colon or end of string,
      // the format is invalid.
      if (*p != '\0' && *p != ':') {
        // Serial.print("fromString: Invalid UID: ");
        // Serial.println(uidString);
        clear();
        return false;
      }
    }

    return uidLength > 0;
  };

  bool fromString(const String& uidString) {
    return fromString(uidString.c_str());
  };

  // ============================================================
  // COMPARISON
  // ============================================================

  bool equals(const Uid& other) const {
    if (uidLength != other.uidLength) {
      return false;
    }

    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] != other.uid[i]) {
        return false;
      }
    }

    return true;
  };

  bool operator==(const Uid& other) const { return this->equals(other); }
  bool operator!=(const Uid& other) const { return !(*this == other); }

  // ============================================================
  // CLEAR
  // ============================================================

  void clear() {
    memset(uid, 0, MAX_LENGTH);
    uidLength = 0;
  };

  // ============================================================
  // VALIDITY
  // ============================================================

  bool isValid() const { return uidLength > 0 && uidLength <= MAX_LENGTH; };

 private:
  uint8_t uid[MAX_LENGTH];
  uint8_t uidLength;
};