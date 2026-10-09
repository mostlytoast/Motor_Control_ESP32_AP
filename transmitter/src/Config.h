#pragma once

#include <Arduino.h>

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


#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240



// =====================================================
// XPT2046 TOUCHSCREEN
// ESP32-2432S028 / CYD
// =====================================================

#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// =====================================================
// LVGL DISPLAY BUFFER
// =====================================================

#define DRAW_BUFFER_LINES 8

// =====================================================
// TOUCH CALIBRATION
// =====================================================

#define TOUCH_MIN_X 220
#define TOUCH_MAX_X 3740

#define TOUCH_MIN_Y 370
#define TOUCH_MAX_Y 3720
