#include "display.h"

// Global/Static Handles
// todo need to be able to access statusLabel in lvdisplay portable
// todo convert espnow to use Json for communication get rid of server state
// todo make espnow shared
// todo need to update everything to use appState and update cycle
LvDisplayController* gDisplayController = nullptr;
Esp32DisplayHost gEsp32DisplayHost;
void Esp32DisplayHost::sendJson(const char* json) {

  if (json == nullptr) {
    return;
  }
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
  getStateFromJson(doc);

}
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touch(XPT2046_CS);
static uint16_t drawBuffer[SCREEN_WIDTH * DRAW_BUFFER_LINES];
// LVGL TICK

static uint32_t lvglTick() { return millis(); }
// LVGL Objects
lv_obj_t* controllerButton = NULL;
lv_obj_t* trackScannerButton = NULL;
lv_obj_t* controllerStatus = NULL;
lv_indev_t* touchIndev = NULL;

// Ownership of the sub-menu
ControllerMenu* controllerMenuInstance = nullptr;

void Display::setupDisplay() {
  setupEncoder();
  app.setTargetSpeed(0);
  app.setRampTime(0);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touch.begin(touchscreenSPI);
  touch.setRotation(1);

  lv_init();
  lv_tick_set_cb(lvglTick);

  lv_display_t* display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, drawBuffer, NULL, sizeof(drawBuffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, displayFlush);

  touchIndev = lv_indev_create();
  lv_indev_set_type(touchIndev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touchIndev, touchRead);

  createUI();

  if (!espnowController.setupESPNow(getJsonFromState, getStateFromJson, true)) {
    app.setStatusLabelText("ESP-NOW ERROR");
  }
}

void Display::createUI() {
  lv_obj_t* screen = lv_screen_active();

  if (gDisplayController == NULL) {
    gDisplayController = new LvDisplayController(&gEsp32DisplayHost);
  }
  gDisplayController->init(screen);

  lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BACKGROUND),
                            LV_PART_MAIN);

  // Main Button
  controllerButton = display_.createActionButton(
      screen, "CONTROLLERS", 215, 27, 100, 25,
      [this]() { controllerMenuInstance->toggle(); }, COLOR_MENU_HEADER);

  // Status Label
  controllerStatus = lv_label_create(screen);
  lv_label_set_text(controllerStatus, "No Receiver");
  lv_obj_set_style_text_color(controllerStatus, lv_color_hex(COLOR_STATUS),
                              LV_PART_MAIN);
  lv_obj_align(controllerStatus, LV_ALIGN_TOP_RIGHT, -5, 30);

  // Initialize the Controller Menu component
  controllerMenuInstance = new ControllerMenu(screen);
  controllerMenuInstance->refreshList();

 
}

void Display::UIRefresh() {
  gDisplayController->refresh();

  if (espnowController.getReceiverListNeedsRefresh()) {
    espnowController.setReceiverListNeedsRefresh(false);
    controllerMenuInstance->refreshList();
  }

  espnowController.espnowUpdate();

  uint32_t sleepTime = lv_timer_handler();
  if (sleepTime > 10) sleepTime = 10;
  delay(sleepTime);
}



void displayFlush(lv_display_t* display, const lv_area_t* area,
                  uint8_t* px_map) {
  uint32_t width = area->x2 - area->x1 + 1;
  uint32_t height = area->y2 - area->y1 + 1;
  if (width == 0 || height == 0) {
    lv_display_flush_ready(display);
    return;
  }
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, width, height);
  tft.pushColors(reinterpret_cast<uint16_t*>(px_map), width * height, true);
  tft.endWrite();
  lv_display_flush_ready(display);
}

void touchRead(lv_indev_t* indev, lv_indev_data_t* data) {
  if (!touch.touched()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  TS_Point p = touch.getPoint();
  data->point.x =
      constrain(map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH - 1), 0,
                SCREEN_WIDTH - 1);
  data->point.y =
      constrain(map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT - 1), 0,
                SCREEN_HEIGHT - 1);
  data->state = LV_INDEV_STATE_PRESSED;
}
