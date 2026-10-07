#include "uidLogic.h"

// ============================================================
// CONSTRUCTORS
// ============================================================

Uid::Uid() { clear(); }

Uid::Uid(const uint8_t* uid, uint8_t length) {
  clear();
  set(uid, length);
}

// ============================================================
// SET UID
// ============================================================

void Uid::set(const uint8_t* source, uint8_t length) {
  clear();

  if (source == nullptr) {
    return;
  }

  if (length > MAX_LENGTH) {
    length = MAX_LENGTH;
  }

  memcpy(uid, source, length);
  uidLength = length;
}

// ============================================================
// STRING TO UID
// ============================================================

bool Uid::fromString(const char* uidString) {
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
}

// ============================================================
// GETTERS
// ============================================================

const uint8_t* Uid::data() const { return uid; }

uint8_t Uid::length() const { return uidLength; }

// ============================================================
// UID TO STRING
// ============================================================

String Uid::toString() const {
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
}

// ============================================================
// COMPARISON
// ============================================================

bool Uid::equals(const Uid& other) const {
  if (uidLength != other.uidLength) {
    return false;
  }

  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] != other.uid[i]) {
      return false;
    }
  }

  return true;
}

// ============================================================
// CLEAR
// ============================================================

void Uid::clear() {
  memset(uid, 0, MAX_LENGTH);
  uidLength = 0;
}

// ============================================================
// VALIDITY
// ============================================================

bool Uid::isValid() const { return uidLength > 0 && uidLength <= MAX_LENGTH; }