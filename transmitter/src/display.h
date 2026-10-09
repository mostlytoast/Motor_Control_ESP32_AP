#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "../../../shared/LvDisplay/lvdisplay_portable.h"
#include "../../../shared/utils/MacUtils.h"
#include "../../shared/SharedConfig.h"
#include "../../shared/communication/espnow_controller.h"
#include "../../shared/communication/packets.h"
#include "../../shared/utils/AppState.h"
#include "../../shared/utils/StateJson.h"
#include "Config.h"
#include "encoder.h"

extern TFT_eSPI tft;

// =====================================================
// XPT2046 TOUCHSCREEN
// ESP32-2432S028 / CYD
// =====================================================

extern SPIClass touchscreenSPI;
extern XPT2046_Touchscreen touch;

// =====================================================
// LVGL DISPLAY BUFFER
// =====================================================

// =====================================================
// LVGL OBJECTS
// =====================================================

extern lv_obj_t* controllerButton;
extern lv_obj_t* controllerMenu;
extern lv_obj_t* controllerList;
extern lv_obj_t* controllerStatus;

extern lv_indev_t* touchIndev;

// =====================================================
// DISPLAY / TOUCH
// =====================================================
void updateUIFromServerState();
void setupDisplay();
void UIRefresh();
void displayFlush(lv_display_t* display, const lv_area_t* area,
                  uint8_t* px_map);

void touchRead(lv_indev_t* indev, lv_indev_data_t* data);

// =====================================================
// DISPLAY UPDATE FUNCTIONS
// =====================================================

// void updateSpeedDisplay();
// void updateRampDisplay();
// void updateMotorStatus();
// void updateControllerStatus();

// =====================================================
// CONTROLLER STATE
// =====================================================

// void resetControllerState();
void closeControllerMenu();
void refreshControllerList();

// =====================================================
// UI
// =====================================================

lv_obj_t* createButton(lv_obj_t* parent, const char* text, int x, int y,
                       int width, int height, lv_event_cb_t callback,
                       uint32_t normalColor);

void createControllerMenu(lv_obj_t* screen);

void createUI();

void UIRefresh();

void selectReceiver(int index);

void processDiscovery();

// =====================================================
// ESP-NOW EVENT PROCESSING
// =====================================================

void processESPNowEvents();

// =====================================================
// HOST WRAPPER
// =====================================================

#include "lvdisplay_portable.h"

extern LvDisplayController* gDisplayController;

class Esp32DisplayHost : public ILvDisplayHost {
 public:
  // void sendCommand(const char* command, int value) override;
  void sendJson(const char* json) override;
};

extern Esp32DisplayHost gEsp32DisplayHost;

#endif  // DISPLAY_H
