#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

using byte = uint8_t;
using word = uint16_t;

// Arduino number bases
#ifndef BIN
#define BIN 2
#endif
#ifndef OCT
#define OCT 8
#endif
#ifndef DEC
#define DEC 10
#endif
#ifndef HEX
#define HEX 16
#endif

// Arduino-compatible timing functions
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);

// Minimal Arduino String compatibility
class String {
 public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String(const std::string& value) : value_(value) {}
  String(char value) : value_(1, value) {}

  String(bool value) : value_(value ? "1" : "0") {}

  String(int value) : value_(std::to_string(value)) {}
  String(unsigned int value) : value_(std::to_string(value)) {}
  String(long value) : value_(std::to_string(value)) {}
  String(unsigned long value) : value_(std::to_string(value)) {}
  String(long long value) : value_(std::to_string(value)) {}
  String(unsigned long long value) : value_(std::to_string(value)) {}
  String(float value) : value_(std::to_string(value)) {}
  String(double value) : value_(std::to_string(value)) {}

  // Arduino-style numeric constructors with an output base.
  String(unsigned char value, unsigned char base = DEC);
  String(int value, unsigned char base);
  String(unsigned int value, unsigned char base);
  String(long value, unsigned char base);
  String(unsigned long value, unsigned char base);
  String(long long value, unsigned char base);
  String(unsigned long long value, unsigned char base);

  const char* c_str() const { return value_.c_str(); }
  size_t length() const { return value_.length(); }
  bool isEmpty() const { return value_.empty(); }
  bool isEmpty() { return value_.empty(); }

  char operator[](size_t index) const { return value_[index]; }
  char& operator[](size_t index) { return value_[index]; }

  String& operator=(const char* value) {
    value_ = value ? value : "";
    return *this;
  }

  String& operator=(const std::string& value) {
    value_ = value;
    return *this;
  }

  String& operator+=(const char* value) {
    if (value) value_ += value;
    return *this;
  }

  String& operator+=(const std::string& value) {
    value_ += value;
    return *this;
  }

  String& operator+=(const String& value) {
    value_ += value.value_;
    return *this;
  }

  String& operator+=(char value) {
    value_ += value;
    return *this;
  }

  String& operator+=(bool value) {
    value_ += value ? "1" : "0";
    return *this;
  }

  String& operator+=(int value) {
    value_ += std::to_string(value);
    return *this;
  }

  String& operator+=(unsigned int value) {
    value_ += std::to_string(value);
    return *this;
  }

  String& operator+=(long value) {
    value_ += std::to_string(value);
    return *this;
  }

  String& operator+=(unsigned long value) {
    value_ += std::to_string(value);
    return *this;
  }

  String& operator+=(float value) {
    value_ += std::to_string(value);
    return *this;
  }

  String& operator+=(double value) {
    value_ += std::to_string(value);
    return *this;
  }

  // Arduino String mutates itself for these operations.
  void toUpperCase();
  void toLowerCase();

  bool startsWith(const String& prefix) const;
  bool endsWith(const String& suffix) const;
  int indexOf(char value) const;
  int indexOf(const char* value) const;
  bool equals(const String& other) const;
  bool equalsIgnoreCase(const String& other) const;

  size_t write(uint8_t value);

  size_t write(const uint8_t* buffer, size_t size);

  size_t write(const char* value);

 private:
  std::string value_;
};

// Arduino-compatible Serial interface.
// Under Emscripten, output is sent to the browser/Node console.
class SerialClass {
 public:
  void begin(unsigned long baud = 115200);
  void end();
  void flush();

  int available() const;
  int availableForWrite() const;
  int peek() const;
  int read();

  size_t write(uint8_t value);
  size_t write(const char* value);
  size_t write(const uint8_t* buffer, size_t size);

  size_t print(const char* value);
  size_t print(char value);
  size_t print(bool value);
  size_t print(int value);
  size_t print(unsigned int value);
  size_t print(long value);
  size_t print(unsigned long value);
  size_t print(long long value);
  size_t print(unsigned long long value);
  size_t print(float value);
  size_t print(double value);
  size_t print(const String& value);

  size_t print(int value, int base);
  size_t print(unsigned int value, int base);
  size_t print(long value, int base);
  size_t print(unsigned long value, int base);

  size_t println();
  size_t println(const char* value);
  size_t println(char value);
  size_t println(bool value);
  size_t println(int value);
  size_t println(unsigned int value);
  size_t println(long value);
  size_t println(unsigned long value);
  size_t println(long long value);
  size_t println(unsigned long long value);
  size_t println(float value);
  size_t println(double value);
  size_t println(const String& value);

  size_t println(int value, int base);
  size_t println(unsigned int value, int base);
  size_t println(long value, int base);
  size_t println(unsigned long value, int base);

  int printf(const char* format, ...);

  explicit operator bool() const { return true; }

 private:
  bool started_ = false;
};

extern SerialClass Serial;