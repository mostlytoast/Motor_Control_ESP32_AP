#pragma once

#include <Arduino.h>
// #include <lvgl.h>
// constexpr char AP_SSID[] = "ESP32-Access-Point";
// constexpr char AP_PASSWORD[] = "12345678";
static constexpr uint8_t MAX_TRACKS = 100;

constexpr uint8_t WIFI_CHANNEL = 6;
constexpr int MAX_CONTROLLERS = 8;
constexpr unsigned long DEFAULT_PAIRING_WINDOW_MS = 60000UL;
constexpr unsigned long ESPNOW_TIMEOUT = 1000;
// TODO have a way for this to be located in the local depending which is being
// compiled???
constexpr char NAME[] = "steam train";

// #define RAMP_STEP 1000
// #define RAMP_MIN 0
// #define RAMP_MAX 10000

constexpr int kRampStep = 1000;
constexpr int kRampMin = 0;
constexpr int kRampMax = 10000;

// TODO standardize color names
struct ColorConfig {
  uint32_t backgroundColor = 0x101820;
  uint32_t labelColor = 0x003249;
  uint32_t textColor = 0xFFFFFF;
  uint32_t inactiveColor = 0x444444;
  uint32_t activeColor = 0x00AA55;
  uint32_t pressedColor = 0x222222;
  uint32_t statusColor = 0x00FFFF;
  uint32_t infoColor = 0x888888;
  uint32_t forwardColor = 0x0077CC;
  uint32_t reverseColor = 0x0077CC;
  uint32_t stopColor = 0xCC2222;
  uint32_t menuHeaderColor = 0x005577;
  uint32_t colorScan = 0xAA6600;
};
static ColorConfig colorConfig;