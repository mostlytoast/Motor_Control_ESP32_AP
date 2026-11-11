#pragma once

#include <Arduino.h>

// constexpr char AP_SSID[] = "ESP32-Access-Point";
// constexpr char AP_PASSWORD[] = "12345678";

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
constexpr uint32_t kColorBackground = 0x101820;
constexpr uint32_t kColorButtonOff = 0x444444;
constexpr uint32_t kColorButtonOn = 0x00AA55;
constexpr uint32_t kColorPressed = 0x222222;
constexpr uint32_t kColorText = 0xFFFFFF;
constexpr uint32_t kColorStatus = 0x00FFFF;
constexpr uint32_t kColorInfo = 0x888888;
constexpr uint32_t kColorForward = 0x0077CC;
constexpr uint32_t kColorReverse = 0x0077CC;
constexpr uint32_t kColorStop = 0xCC2222;
constexpr uint32_t KColorMenuHeader = 0x005577;

#define COLOR_BACKGROUND 0x101820
#define COLOR_BUTTON_OFF 0x444444
#define COLOR_BUTTON_ON 0x00AA55

#define COLOR_FORWARD 0x0077CC
#define COLOR_REVERSE 0x0077CC
#define COLOR_STOP 0xCC2222

#define COLOR_PRESSED 0x222222

#define COLOR_TEXT 0xFFFFFF
#define COLOR_STATUS 0x00FFFF
#define COLOR_INFO 0x888888

#define COLOR_MENU 0x18242D
#define COLOR_MENU_HEADER 0x005577
#define COLOR_SELECTED 0x008844
#define COLOR_SCAN 0xAA6600
