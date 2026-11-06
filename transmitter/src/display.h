#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "../../shared/LvDisplay/SharedDisplayCalls.h"
#include "../../shared/LvDisplay/lvdisplay_portable.h"
#include "../../shared/SharedConfig.h"
#include "../../shared/communication/EspnowController.h"
#include "../../shared/communication/packets.h"
#include "../../shared/utils/AppState.h"
#include "../../shared/utils/MacUtils.h"
#include "../../shared/utils/StateJson.h"
#include "Config.h"
#include "ControllerMenu.h"
#include "encoder.h"
#include "lvdisplay_portable.h"
extern TFT_eSPI tft;

// XPT2046 TOUCHSCREEN
// ESP32-2432S028 / CYD

extern SPIClass touchscreenSPI;
extern XPT2046_Touchscreen touch;

// LVGL DISPLAY BUFFER

// LVGL OBJECTS

extern lv_obj_t* controllerButton;
extern lv_obj_t* trackScannerButton;
extern lv_obj_t* controllerMenu;
extern lv_obj_t* controllerList;
extern lv_obj_t* controllerStatus;

extern lv_indev_t* touchIndev;

// DISPLAY / TOUCH
class Display {
 public:
  void setupDisplay();
  void UIRefresh();

 private:
  // DISPLAY UPDATE FUNCTIONS

  void toggleControllerMenu();
  void updateControllerStatus();

  // CONTROLLER STATE

  // void resetControllerState();
  void closeControllerMenu();
  void refreshControllerList();

  // UI

  void createControllerMenu(lv_obj_t* screen);

  void createUI();
  static void scanEvent(lv_event_t* event);
  static void controllerMenuEvent(lv_event_t* event);
  static void receiverSelectEvent(lv_event_t* event);

  void selectReceiver(Peer* peer);

  // HOST WRAPPER
  SharedDisplayCalls display_;
};

void displayFlush(lv_display_t* display, const lv_area_t* area,
                  uint8_t* px_map);

void touchRead(lv_indev_t* indev, lv_indev_data_t* data);
extern LvDisplayController* gDisplayController;

class Esp32DisplayHost : public ILvDisplayHost {
 public:
  // void sendCommand(const char* command, int value) override;
  void sendJson(const char* json) override;
};

extern Esp32DisplayHost gEsp32DisplayHost;

#endif  // DISPLAY_H
