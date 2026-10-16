
#include "display.h"
// todo need to be able to access statusLabel in lvdisplay portable
// todo convert espnow to use Json for communication get rid of server state
// todo make espnow shared
// todo need to update everything to use appState and update cycle
LvDisplayController* gDisplayController = nullptr;
Esp32DisplayHost gEsp32DisplayHost;

void Esp32DisplayHost::sendJson(const char* json) {
  // get data from display
  if (json == nullptr) {
    return;
  }
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
  getStateFromJson(doc);
  // notifyLocalStateChanged();
  // Serial.print("[LVGL JSON -> ESP32] ");
  // Serial.println(json);
}

// DISPLAY

TFT_eSPI tft = TFT_eSPI();

SPIClass touchscreenSPI = SPIClass(HSPI);

XPT2046_Touchscreen touch(XPT2046_CS);

static uint16_t drawBuffer[SCREEN_WIDTH * DRAW_BUFFER_LINES];

// LVGL TICK

static uint32_t lvglTick() { return millis(); }

// LVGL OBJECTS

lv_obj_t* controllerButton = NULL;
lv_obj_t* controllerMenu = NULL;
lv_obj_t* controllerList = NULL;
lv_obj_t* controllerStatus = NULL;

lv_indev_t* touchIndev = NULL;

// SETUP DISPLAY

void setupDisplay() {
  setupEncoder();

  app.setTargetSpeed(0);

  // SERVER STATE

  app.setRampTime(0);

  // ===================================================
  // TFT
  // ===================================================

  tft.begin();

  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);

  // ===================================================
  // TOUCH
  // ===================================================

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);

  touch.begin(touchscreenSPI);

  touch.setRotation(1);

  Serial.println("Touch initialized");

  // ===================================================
  // LVGL
  // ===================================================

  lv_init();

  lv_tick_set_cb(lvglTick);

  Serial.print("LVGL version: ");

  Serial.print(LVGL_VERSION_MAJOR);

  Serial.print(".");

  Serial.print(LVGL_VERSION_MINOR);

  Serial.print(".");

  Serial.println(LVGL_VERSION_PATCH);

  // ===================================================
  // LVGL DISPLAY
  // ===================================================

  lv_display_t* display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);

  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

  lv_display_set_buffers(display, drawBuffer, NULL, sizeof(drawBuffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_display_set_flush_cb(display, displayFlush);

  // ===================================================
  // LVGL TOUCH
  // ===================================================

  touchIndev = lv_indev_create();

  if (touchIndev == NULL) {
    Serial.println("ERROR: LVGL input device creation failed");
  }

  else {
    lv_indev_set_type(touchIndev, LV_INDEV_TYPE_POINTER);

    lv_indev_set_read_cb(touchIndev, touchRead);

    Serial.println("LVGL touch ready");
  }

  // ===================================================
  // UI
  // ===================================================

  createUI();

  lv_refr_now(display);

  // ===================================================
  // ESP-NOW
  // ===================================================

  if (!espnowController.setupESPNow(true)) {
    app.setStatusLabelText("ESP-NOW ERROR");

    return;
  }

  // ===================================================
  // DISCOVERY
  // ===================================================
  // todo add to appstate?

  // lastSpeedSend = millis();
}

// UPDATE CONTROLLER STATUS

// TODO split into espnow
void updateControllerStatus() {
  if (controllerStatus == NULL) {
    return;
  }

  if (espnowController.hasActiveReceiver()) {
    lv_label_set_text(controllerStatus, "No Receiver");

    return;
  }

  char text[64];

  snprintf(text, sizeof(text), "CTRL: %s",
           espnowController.getReceivers()[espnowController.getActiveReceiver()]
               .name);

  lv_label_set_text(controllerStatus, text);
}

// CLOSE CONTROLLER MENU

void closeControllerMenu() {
  if (controllerMenu != NULL) {
    lv_obj_set_hidden(controllerMenu, true);
  }
}

// RECEIVER SELECT EVENT

static void receiverSelectEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  intptr_t index = (intptr_t)lv_event_get_user_data(event);

  selectReceiver((int)index);

  closeControllerMenu();
}

// SCAN EVENT lv

// TODO move to espnow
static void scanEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  // Reset local UI

  // resetControllerState();

  // Stop current receiver

  // if (activeReceiver >= 0) {
  // TODO is this needed?
  //   stopActiveReceiver();
  // }

  // Remove receiver peers

  espnowController.removePeers();

  espnowController.resetDiscover();
  if (controllerStatus != NULL) {
    lv_label_set_text(controllerStatus, "Scanning...");
  }

  app.setStatusLabelText("STOPPED");

  Serial.println("Starting receiver discovery...");
}

// CONTROLLER MENU EVENT

static void controllerMenuEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  if (controllerMenu == NULL) {
    return;
  }

  if (lv_obj_is_hidden(controllerMenu)) {
    lv_obj_set_hidden(controllerMenu, false);

    espnowController.setReceiverListNeedsRefresh(true);
  }

  else {
    closeControllerMenu();
  }
}

// REFRESH CONTROLLER LIST lv

// TODO split so some functionality is in espnow
void refreshControllerList() {
  if (controllerList == NULL) {
    return;
  }

  lv_obj_clean(controllerList);

  // Scan button

  lv_obj_t* scanButton = lv_button_create(controllerList);

  lv_obj_set_size(scanButton, 280, 36);

  lv_obj_set_style_bg_color(scanButton, lv_color_hex(COLOR_SCAN), LV_PART_MAIN);

  lv_obj_t* scanLabel = lv_label_create(scanButton);

  lv_label_set_text(scanLabel, "SCAN / DISCOVER");

  lv_obj_center(scanLabel);

  lv_obj_add_event_cb(scanButton, scanEvent, LV_EVENT_CLICKED, NULL);

  // Receiver buttons

  for (int i = 0; i < espnowController.getNumberOfReceivers(); i++) {
    if (!espnowController.getReceivers()[i].used) {
      continue;
    }

    lv_obj_t* button = lv_button_create(controllerList);

    lv_obj_set_size(button, 280, 40);

    bool selected = espnowController.isActiveReceiver(i);

    lv_obj_set_style_bg_color(button,
                              selected ? lv_color_hex(COLOR_SELECTED)
                                       : lv_color_hex(COLOR_BUTTON_OFF),
                              LV_PART_MAIN);

    char text[64];

    snprintf(text, sizeof(text), "%s%s", selected ? "> " : "",
             espnowController.getReceivers()[i].name);

    lv_obj_t* label = lv_label_create(button);

    lv_label_set_text(label, text);

    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);

    lv_obj_center(label);

    lv_obj_add_event_cb(button, receiverSelectEvent, LV_EVENT_CLICKED,
                        (void*)(intptr_t)i);
  }

  // Empty list

  if (espnowController.getNumberOfReceivers() == 0) {
    lv_obj_t* none = lv_label_create(controllerList);

    lv_label_set_text(none, "No receivers found");

    lv_obj_set_style_text_color(none, lv_color_hex(COLOR_INFO), LV_PART_MAIN);
  }
}

// CREATE BUTTON

lv_obj_t* createButton(lv_obj_t* parent, const char* text, int x, int y,
                       int width, int height, lv_event_cb_t callback,
                       uint32_t normalColor) {
  // TODO see if you can use create button function from lvdisplay in shared
  // file?
  lv_obj_t* button = lv_button_create(parent);

  lv_obj_set_pos(button, x, y);

  lv_obj_set_size(button, width, height);

  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);

  lv_obj_set_style_bg_color(button, lv_color_hex(normalColor),
                            LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_PRESSED),
                            LV_PART_MAIN | LV_STATE_PRESSED);

  lv_obj_set_style_bg_color(button, lv_color_hex(normalColor),
                            LV_PART_MAIN | LV_STATE_CHECKED);

  lv_obj_t* label = lv_label_create(button);

  lv_label_set_text(label, text);

  lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);

  lv_obj_center(label);

  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);

  return button;
}

// CREATE CONTROLLER MENU

void createControllerMenu(lv_obj_t* screen) {
  controllerMenu = lv_obj_create(screen);

  lv_obj_set_size(controllerMenu, 310, 225);

  lv_obj_center(controllerMenu);

  lv_obj_set_style_bg_color(controllerMenu, lv_color_hex(COLOR_MENU),
                            LV_PART_MAIN);

  lv_obj_set_style_border_color(controllerMenu, lv_color_hex(COLOR_STATUS),
                                LV_PART_MAIN);

  lv_obj_set_style_border_width(controllerMenu, 2, LV_PART_MAIN);

  // Header

  lv_obj_t* header = lv_label_create(controllerMenu);

  lv_label_set_text(header, "SELECT RECEIVER");

  lv_obj_set_style_text_color(header, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);

  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 5);

  // List

  controllerList = lv_obj_create(controllerMenu);

  lv_obj_set_size(controllerList, 290, 180);

  lv_obj_align(controllerList, LV_ALIGN_TOP_MID, 0, 32);

  lv_obj_set_style_bg_opa(controllerList, LV_OPA_TRANSP, LV_PART_MAIN);

  lv_obj_set_style_border_width(controllerList, 0, LV_PART_MAIN);

  lv_obj_set_flex_flow(controllerList, LV_FLEX_FLOW_COLUMN);

  lv_obj_set_style_pad_row(controllerList, 5, LV_PART_MAIN);

  espnowController.setReceiverListNeedsRefresh(true);

  lv_obj_set_hidden(controllerMenu, true);
}

// CREATE UI

void createUI() {
  lv_obj_t* screen = lv_screen_active();

  if (gDisplayController == NULL) {
    gDisplayController = new LvDisplayController(&gEsp32DisplayHost);
  }

  gDisplayController->init(screen);

  // legacy UI still keeps the original controller list/menu layout
  lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BACKGROUND),
                            LV_PART_MAIN);

  lv_obj_t* title = lv_label_create(screen);

  controllerButton = createButton(screen, "CONTROLLERS", 215, 27, 100, 25,
                                  controllerMenuEvent, COLOR_MENU_HEADER);

  controllerStatus = lv_label_create(screen);
  lv_label_set_text(controllerStatus, "No Receiver");
  lv_obj_set_style_text_color(controllerStatus, lv_color_hex(COLOR_STATUS),
                              LV_PART_MAIN);
  lv_obj_align(controllerStatus, LV_ALIGN_TOP_RIGHT, -5, 30);

  createControllerMenu(screen);
}

void UIRefresh() {
  // Apply receiver state to UI
  // todo improve this
  gDisplayController->refresh();

  // if (app.getServerStateChanged()) {
  //   gDisplayController->refresh();
  // }

  // Refresh receiver list

  if (espnowController.getReceiverListNeedsRefresh()) {
    espnowController.setReceiverListNeedsRefresh(false);

    refreshControllerList();
  }

  // ESP-NOW periodic motor/heartbeat traffic

  espnowController.espnowUpdate();

  // LVGL

  uint32_t sleepTime = lv_timer_handler();

  if (sleepTime > 10) {
    sleepTime = 10;
  }

  delay(sleepTime);
}

// SELECT RECEIVER

void selectReceiver(int index) {
  switch (espnowController.validateSelectedPeer(index)) {
    case INVALID_INDEX:
      return;
    case ALREADY_SELECTED:
      closeControllerMenu();
      return;
    case PEER_ERROR:
      app.setStatusLabelText("PEER ERROR");
      return;
    case PASS_CHECK:
      break;
    default:
      break;
  }
  // Reset local controls

  // resetControllerState();

  updateControllerStatus();

  app.setStatusLabelText("PAIRING...");

  // Send pairing request
  switch (espnowController.sendPairRequest(index)) {
    case PAIR_ERROR:
      app.setStatusLabelText("PAIR FAILED");
      return;
    case PAIR_SUCCESS:
      app.setStatusLabelText("PAIR SUCCESS");
      break;
    default:
      break;
  }
}

// DISPLAY FLUSH

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

// TOUCH READ

void touchRead(lv_indev_t* indev, lv_indev_data_t* data) {
  if (!touch.touched()) {
    data->state = LV_INDEV_STATE_RELEASED;

    return;
  }

  TS_Point p = touch.getPoint();

  int16_t x = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH - 1);

  int16_t y = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT - 1);

  x = constrain(x, 0, SCREEN_WIDTH - 1);

  y = constrain(y, 0, SCREEN_HEIGHT - 1);

  data->point.x = x;

  data->point.y = y;

  data->state = LV_INDEV_STATE_PRESSED;
}
