#pragma once

#include <Arduino.h>
// #include <Arduino.h> cant include as this script gets imported by map which is used in webassm
#include <stdint.h>
#include <string.h>

class Uid {
 public:
  static constexpr uint8_t MAX_LENGTH = 7;

  // ============================================================
  // CONSTRUCTORS
  // ============================================================

  Uid();
  Uid(const uint8_t* uid, uint8_t length);

  // ============================================================
  // SETTERS
  // ============================================================

  void set(const uint8_t* uid, uint8_t length);
  bool fromString(const char* uidString);

  // ============================================================
  // GETTERS
  // ============================================================

  const uint8_t* data() const;
  uint8_t length() const;

  // ============================================================
  // CONVERSION
  // ============================================================

  String toString() const;

  // ============================================================
  // COMPARISON
  // ============================================================

  bool equals(const Uid& other) const;

  // ============================================================
  // UTILITIES
  // ============================================================

  void clear();
  bool isValid() const;

 private:
  uint8_t uid[MAX_LENGTH];
  uint8_t uidLength;
};