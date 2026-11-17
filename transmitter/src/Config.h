#pragma once
#if defined(BOARD_DEV)

// #include "ConfigEsp32.h"
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
#define ENCODER_A 15
#define ENCODER_B 16

#elif defined(BOARD_S3)
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

// #include "ConfigEsp32s3.h"



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


#define TOUCH_SDA 16
#define TOUCH_SCL 15
#define TOUCH_RST 18
#define TOUCH_INT 17

#define ENCODER_A 43
#define ENCODER_B 44
#else

#error "No board configuration selected! Define BOARD_A or BOARD_B."

#endif

#include <Arduino.h>
