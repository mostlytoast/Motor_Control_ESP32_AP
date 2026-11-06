#pragma once

#include <Arduino.h>

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
