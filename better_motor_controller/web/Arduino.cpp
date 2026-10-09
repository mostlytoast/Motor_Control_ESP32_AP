#include "Arduino.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <chrono>
#include <iostream>
#include <thread>
#endif

namespace {

std::string numberToBase(unsigned long long value, unsigned int base) {
  if (base < 2 || base > 36) base = DEC;

  const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  char buffer[65];
  size_t pos = sizeof(buffer) - 1;
  buffer[pos] = '\0';

  do {
    buffer[--pos] = digits[value % base];
    value /= base;
  } while (value != 0);

  return std::string(&buffer[pos]);
}

template <typename T>
std::string signedNumberToBase(T value, unsigned int base) {
  if (base < 2 || base > 36) base = DEC;

  using U = typename std::make_unsigned<T>::type;
  bool negative = value < 0;
  U magnitude;

  if (negative) {
    magnitude = static_cast<U>(-(value + 1));
    ++magnitude;
  } else {
    magnitude = static_cast<U>(value);
  }

  std::string result =
      numberToBase(static_cast<unsigned long long>(magnitude), base);
  if (negative) result.insert(result.begin(), '-');
  return result;
}

}  // namespace

// -----------------------------------------------------------------------------
// String numeric/base constructors
// -----------------------------------------------------------------------------

String::String(unsigned char value, unsigned char base)
    : value_(numberToBase(value, base)) {}

String::String(int value, unsigned char base)
    : value_(signedNumberToBase(value, base)) {}

String::String(unsigned int value, unsigned char base)
    : value_(numberToBase(value, base)) {}

String::String(long value, unsigned char base)
    : value_(signedNumberToBase(value, base)) {}

String::String(unsigned long value, unsigned char base)
    : value_(numberToBase(value, base)) {}

String::String(long long value, unsigned char base)
    : value_(signedNumberToBase(value, base)) {}

String::String(unsigned long long value, unsigned char base)
    : value_(numberToBase(value, base)) {}

void String::toUpperCase() {
  std::transform(
      value_.begin(), value_.end(), value_.begin(),
      [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
}

void String::toLowerCase() {
  std::transform(
      value_.begin(), value_.end(), value_.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

bool String::startsWith(const String& prefix) const {
  return value_.size() >= prefix.value_.size() &&
         value_.compare(0, prefix.value_.size(), prefix.value_) == 0;
}

bool String::endsWith(const String& suffix) const {
  return value_.size() >= suffix.value_.size() &&
         value_.compare(value_.size() - suffix.value_.size(),
                        suffix.value_.size(), suffix.value_) == 0;
}

int String::indexOf(char value) const {
  const size_t position = value_.find(value);
  return position == std::string::npos ? -1 : static_cast<int>(position);
}

int String::indexOf(const char* value) const {
  if (!value) return -1;
  const size_t position = value_.find(value);
  return position == std::string::npos ? -1 : static_cast<int>(position);
}

bool String::equals(const String& other) const {
  return value_ == other.value_;
}

bool String::equalsIgnoreCase(const String& other) const {
  if (value_.size() != other.value_.size()) return false;

  for (size_t i = 0; i < value_.size(); ++i) {
    const unsigned char a = static_cast<unsigned char>(value_[i]);
    const unsigned char b = static_cast<unsigned char>(other.value_[i]);
    if (std::tolower(a) != std::tolower(b)) return false;
  }

  return true;
}
// -----------------------------------------------------------------------------
// String output / Writer compatibility
// -----------------------------------------------------------------------------

size_t String::write(uint8_t value) {
  value_.push_back(static_cast<char>(value));
  return 1;
}

size_t String::write(const uint8_t* buffer, size_t size) {
  if (buffer == nullptr || size == 0) {
    return 0;
  }

  value_.append(reinterpret_cast<const char*>(buffer), size);

  return size;
}

size_t String::write(const char* value) {
  if (value == nullptr) {
    return 0;
  }

  const size_t size = std::strlen(value);
  value_.append(value, size);
  return size;
}

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__

unsigned long millis() {
  return static_cast<unsigned long>(emscripten_get_now());
}

unsigned long micros() {
  return static_cast<unsigned long>(emscripten_get_now() * 1000.0);
}

void delay(unsigned long ms) { emscripten_sleep(ms); }

#else

unsigned long millis() {
  static const auto start = std::chrono::steady_clock::now();
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start)
          .count());
}

unsigned long micros() {
  static const auto start = std::chrono::steady_clock::now();
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - start)
          .count());
}

void delay(unsigned long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

#endif

// -----------------------------------------------------------------------------
// Serial
// -----------------------------------------------------------------------------

SerialClass Serial;

namespace {

void consoleOutput(const char* text) {
  if (!text) return;

#ifdef __EMSCRIPTEN__
  emscripten_log(EM_LOG_CONSOLE, "%s", text);
#else
  std::cout << text;
  std::cout.flush();
#endif
}

template <typename T>
size_t numberOutput(T value) {
  const std::string s = std::to_string(value);
  consoleOutput(s.c_str());
  return s.size();
}

template <typename T>
size_t numberOutputBase(T value, int base) {
  const std::string s = signedNumberToBase(value, base);
  consoleOutput(s.c_str());
  return s.size();
}

template <typename T>
size_t unsignedNumberOutputBase(T value, int base) {
  const std::string s = numberToBase(value, base);
  consoleOutput(s.c_str());
  return s.size();
}

}  // namespace

void SerialClass::begin(unsigned long) { started_ = true; }
void SerialClass::end() { started_ = false; }

void SerialClass::flush() {
#ifndef __EMSCRIPTEN__
  std::cout.flush();
#endif
}

int SerialClass::available() const { return 0; }
int SerialClass::availableForWrite() const { return 1024; }
int SerialClass::peek() const { return -1; }
int SerialClass::read() { return -1; }

size_t SerialClass::write(uint8_t value) {
  char s[2] = {static_cast<char>(value), '\0'};
  consoleOutput(s);
  return 1;
}

size_t SerialClass::write(const char* value) {
  if (!value) return 0;
  consoleOutput(value);
  return std::strlen(value);
}

size_t SerialClass::write(const uint8_t* buffer, size_t size) {
  if (!buffer || !size) return 0;

#ifdef __EMSCRIPTEN__
  std::string s(reinterpret_cast<const char*>(buffer), size);
  consoleOutput(s.c_str());
#else
  std::cout.write(reinterpret_cast<const char*>(buffer),
                  static_cast<std::streamsize>(size));
  std::cout.flush();
#endif

  return size;
}

size_t SerialClass::print(const char* v) { return write(v); }
size_t SerialClass::print(char v) { return write(static_cast<uint8_t>(v)); }
size_t SerialClass::print(bool v) { return write(v ? "1" : "0"); }
size_t SerialClass::print(int v) { return numberOutput(v); }
size_t SerialClass::print(unsigned int v) { return numberOutput(v); }
size_t SerialClass::print(long v) { return numberOutput(v); }
size_t SerialClass::print(unsigned long v) { return numberOutput(v); }
size_t SerialClass::print(long long v) { return numberOutput(v); }
size_t SerialClass::print(unsigned long long v) { return numberOutput(v); }
size_t SerialClass::print(float v) { return numberOutput(v); }
size_t SerialClass::print(double v) { return numberOutput(v); }
size_t SerialClass::print(const String& v) { return write(v.c_str()); }
size_t SerialClass::print(int v, int base) { return numberOutputBase(v, base); }
size_t SerialClass::print(unsigned int v, int base) {
  return unsignedNumberOutputBase(v, base);
}
size_t SerialClass::print(long v, int base) {
  return numberOutputBase(v, base);
}
size_t SerialClass::print(unsigned long v, int base) {
  return unsignedNumberOutputBase(v, base);
}

size_t SerialClass::println() { return write("\n"); }
size_t SerialClass::println(const char* v) { return print(v) + println(); }
size_t SerialClass::println(char v) { return print(v) + println(); }
size_t SerialClass::println(bool v) { return print(v) + println(); }
size_t SerialClass::println(int v) { return print(v) + println(); }
size_t SerialClass::println(unsigned int v) { return print(v) + println(); }
size_t SerialClass::println(long v) { return print(v) + println(); }
size_t SerialClass::println(unsigned long v) { return print(v) + println(); }
size_t SerialClass::println(long long v) { return print(v) + println(); }
size_t SerialClass::println(unsigned long long v) {
  return print(v) + println();
}
size_t SerialClass::println(float v) { return print(v) + println(); }
size_t SerialClass::println(double v) { return print(v) + println(); }
size_t SerialClass::println(const String& v) { return print(v) + println(); }
size_t SerialClass::println(int v, int base) {
  return print(v, base) + println();
}
size_t SerialClass::println(unsigned int v, int base) {
  return print(v, base) + println();
}
size_t SerialClass::println(long v, int base) {
  return print(v, base) + println();
}
size_t SerialClass::println(unsigned long v, int base) {
  return print(v, base) + println();
}

int SerialClass::printf(const char* format, ...) {
  if (!format) return 0;

  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);

  const int length = std::vsnprintf(nullptr, 0, format, copy);
  va_end(copy);

  if (length <= 0) {
    va_end(args);
    return length;
  }

  std::string buffer(static_cast<size_t>(length) + 1, '\0');
  std::vsnprintf(buffer.data(), buffer.size(), format, args);
  va_end(args);

  consoleOutput(buffer.c_str());
  return length;
}