#pragma once

#include <Arduino.h>

// constexpr char AP_SSID[] = "ESP32-Access-Point";
// constexpr char AP_PASSWORD[] = "12345678";

constexpr uint8_t WIFI_CHANNEL = 6;
constexpr int MAX_CONTROLLERS = 8;
constexpr unsigned long DEFAULT_PAIRING_WINDOW_MS = 60000UL;
constexpr unsigned long ESPNOW_TIMEOUT = 500;
// TODO have a way for this to be located in the local depending which is being compiled???
constexpr char RECEIVER_NAME[] = "steam train";

#define RAMP_STEP 1000
#define RAMP_MIN 0
#define RAMP_MAX 10000
