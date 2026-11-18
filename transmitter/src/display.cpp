
#include "display.h"

#if defined(BOARD_DEV) && defined(BOARD_S3)

#error "Only one board configuration can be selected!"

#elif defined(BOARD_DEV)

#include <XPT2046_Touchscreen.h>

#elif defined(BOARD_S3)

#include <Wire.h>

#else

#error "No board configuration selected! Define BOARD_DEV or BOARD_S3."

#endif

// ============================================================
// GLOBAL HANDLES
// ============================================================

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

  if (error) {
    Serial.print("[Display] JSON error: ");
    Serial.println(error.c_str());
    return;
  }

  getStateFromCondensedJson(doc);
}

// ============================================================
// DISPLAY
// ============================================================

TFT_eSPI tft = TFT_eSPI();

#if defined(BOARD_DEV)

// Original resistive touchscreen (SPI)
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touch(XPT2046_CS);

#elif defined(BOARD_S3)

// Capacitive touchscreen (FT6336U, I2C)

namespace {

constexpr uint8_t TOUCH_I2C_ADDRESS = 0x38;

// Native touchscreen resolution (portrait)

constexpr uint16_t TOUCH_NATIVE_WIDTH = SCREEN_HEIGHT;
constexpr uint16_t TOUCH_NATIVE_HEIGHT = SCREEN_WIDTH;

constexpr uint8_t FT6336_REG_TOUCH_POINTS = 0x02;

bool touchReady = false;

// Read multiple consecutive FT6336U registers.
bool readFT6336Registers(uint8_t startRegister, uint8_t* buffer,
                         uint8_t length) {
  Wire.beginTransmission(TOUCH_I2C_ADDRESS);

  Wire.write(startRegister);

  // Keep the bus active for the subsequent read.
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t received = Wire.requestFrom(TOUCH_I2C_ADDRESS, length, (uint8_t)true);

  if (received != length) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  for (uint8_t i = 0; i < length; i++) {
    if (!Wire.available()) {
      return false;
    }

    buffer[i] = Wire.read();
  }

  return true;
}

}  // namespace

#endif

// ============================================================
// LVGL BUFFER / TICK
// ============================================================

static uint16_t drawBuffer[SCREEN_WIDTH * DRAW_BUFFER_LINES];

static uint32_t lvglTick() { return millis(); }

// ============================================================
// LVGL OBJECTS
// ============================================================

lv_obj_t* controllerButton = NULL;
lv_obj_t* statusLabel_ = NULL;
lv_obj_t* trackScannerButton = NULL;
lv_obj_t* controllerStatus = NULL;

lv_indev_t* touchIndev = NULL;

// Ownership of the sub-menu
ControllerMenu* controllerMenuInstance = nullptr;

// ============================================================
// TOUCH INITIALIZATION
// ============================================================

static bool initTouch() {
#if defined(BOARD_DEV)

  // Original XPT2046 SPI touchscreen initialization.

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);

  touch.begin(touchscreenSPI);
  touch.setRotation(1);

  Serial.println("[Touch] XPT2046 initialized");

  return true;

#elif defined(BOARD_S3)

  // FT6336U capacitive touchscreen initialization.

  Serial.println("[Touch] Initializing FT6336U...");

  // Reset the capacitive touchscreen.
  pinMode(TOUCH_RST, OUTPUT);

  digitalWrite(TOUCH_RST, LOW);
  delay(10);

  digitalWrite(TOUCH_RST, HIGH);
  delay(200);

  // Interrupt is active-low. We poll I2C from LVGL,
  // so an interrupt handler is not necessary.
  pinMode(TOUCH_INT, INPUT_PULLUP);

  // Initialize I2C using the board's actual pins.
  if (!Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000)) {
    Serial.println("[Touch] ERROR: I2C initialization failed");

    touchReady = false;
    return false;
  }

  // Verify that the FT6336U responds at address 0x38.
  Wire.beginTransmission(TOUCH_I2C_ADDRESS);

  if (Wire.endTransmission() != 0) {
    Serial.println("[Touch] ERROR: FT6336U not detected at 0x38");

    touchReady = false;
    return false;
  }

  touchReady = true;

  Serial.println("[Touch] FT6336U initialized successfully");
  Serial.println("[Touch] I2C address: 0x38");

  return true;

#endif
}

// ============================================================
// DISPLAY INITIALIZATION
// ============================================================

void Display::setupDisplay() {
  if (!espnowController.setupESPNow(
          getCondensedJsonFromStateTransmitter, getStateFromCondensedJson,
          [this]() { this->reset(); }, true)) {
    app.setStatusLabelText("ERROR");
  }

  app.setTargetSpeed(0);
  app.setRampTime(0);
  Serial.println("test");
  // Initialize TFT.
  Serial.println("\n========== TFT INITIALIZATION ==========");

  // Print the configuration TFT_eSPI actually compiled with.
  setup_t tftSetup;
  tft.getSetup(tftSetup);

  tft.init();

  tft.setRotation(1);

#if defined(BOARD_S3)
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH);
#endif

  tft.fillScreen(TFT_BLACK);

  // Initialize the selected touchscreen.
  if (!initTouch()) {
    Serial.println("[Display] WARNING: Touchscreen unavailable");
  }

  // Initialize LVGL.
  lv_init();

  lv_tick_set_cb(lvglTick);

  lv_display_t* display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);

  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

  lv_display_set_buffers(display, drawBuffer, NULL, sizeof(drawBuffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_display_set_flush_cb(display, displayFlush);

  // Initialize LVGL input device.
  touchIndev = lv_indev_create();

  lv_indev_set_type(touchIndev, LV_INDEV_TYPE_POINTER);

  lv_indev_set_read_cb(touchIndev, touchRead);

  createUI();

  // TODO need to make it so it does not overwrite the track state of the train,
  // need to do that by not including track for the getCondensedJsonFromState
  // make custom one.
}

// ============================================================
// RESET
// ============================================================

// TODO should just be able to only call app.reset() and everything gets reset

void Display::reset() {
  app.reset();

  // gDisplayController->reset();
  // refresh();
}

// ============================================================
// CREATE UI
// ============================================================

void Display::createUI() {
  lv_obj_t* screen = lv_screen_active();

  if (screen == nullptr) {
    Serial.println("[Display] ERROR: Active screen is null");
    return;
  }

  if (gDisplayController == nullptr) {
    gDisplayController = new LvDisplayController(&gEsp32DisplayHost);
  }

  // Set screen background.
  lv_obj_set_style_bg_color(screen, lv_color_hex(colorConfig.backgroundColor),
                            LV_PART_MAIN);

  // Initialize motor controls.
  // Keep the original screen argument.

  lv_obj_t* mainGrid = gDisplayController->init(screen);
  if (!mainGrid) {
    Serial.println("failed to get main grid");
    return;
  }
  // ------------------------------------------------------------
  // PEERS BUTTON
  // ------------------------------------------------------------

  controllerButton = sharedDisplay.createActionButton(
      mainGrid, "Peers",
      [this](lv_event_t* event) {
        if (controllerMenuInstance != nullptr) {
          controllerMenuInstance->toggle();
        }
      },
      lv_color_hex(colorConfig.menuHeaderColor));

  lv_obj_set_grid_cell(controllerButton, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);

  controllerMenuInstance = new ControllerMenu(screen);
  controllerMenuInstance->refreshList();

  // ------------------------------------------------------------
  // STATUS LABEL
  // ------------------------------------------------------------

  statusLabel_ = sharedDisplay.createLabel(
      mainGrid, lv_color_hex(colorConfig.statusColor), 0, 0, LV_PART_MAIN,
      LV_ALIGN_CENTER, [this]() -> const char* {
        if (!app.getStatusLabelText().equals("")) {
          static char text[32];

          std::snprintf(text, sizeof(text), "%s",
                        app.getStatusLabelText().c_str());

          return text;
        }

        return "NO PEER";
      });

  lv_obj_set_grid_cell(statusLabel_, LV_GRID_ALIGN_CENTER, 2, 1,
                       LV_GRID_ALIGN_CENTER, 2, 1);
  lv_obj_move_foreground(controllerButton);
  lv_obj_move_to_index(controllerButton, -1);
  lv_obj_move_to_index(statusLabel_, -1);

  Serial.println("[Display] UI creation finished");
}

// ============================================================
// REFRESH
// ============================================================

unsigned long lastRefresh = 0;

const unsigned long refreshWaitTime = 10;

void Display::refresh() {
  // sharedDisplay.updateStates();

  // Serial.print("Free Heap: ");
  // Serial.println(ESP.getFreeHeap());

  if (espnowController.getReceiverListNeedsRefresh()) {
    espnowController.setReceiverListNeedsRefresh(false);

    controllerMenuInstance->refreshList();
  }

  espnowController.espnowUpdate();

  uint32_t sleepTime = lv_timer_handler();

  if (sleepTime > 5) {
    sleepTime = 5;
  }

  delay(sleepTime);

  if ((millis() - lastRefresh) > refreshWaitTime) {
    sharedDisplay.updateStates();
    controllerMenuInstance->refresh();
    gDisplayController->refresh();
    lastRefresh = millis();
  }
}

// ============================================================
// DISPLAY FLUSH
// ============================================================

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

// ============================================================
// TOUCH READ
// ============================================================

void touchRead(lv_indev_t* indev, lv_indev_data_t* data) {
#if defined(BOARD_DEV)

  // ----------------------------------------------------------
  // ORIGINAL XPT2046 RESISTIVE TOUCHSCREEN
  // ----------------------------------------------------------

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

#elif defined(BOARD_S3)

  // ----------------------------------------------------------
  // FT6336U CAPACITIVE TOUCHSCREEN (I2C)
  // ----------------------------------------------------------

  if (!touchReady) {
    data->state = LV_INDEV_STATE_RELEASED;

    return;
  }

  // Read:
  // 0x02 = number of touch points
  // 0x03 = first point X high
  // 0x04 = first point X low
  // 0x05 = first point Y high
  // 0x06 = first point Y low

  uint8_t touchData[5];

  if (!readFT6336Registers(FT6336_REG_TOUCH_POINTS, touchData,
                           sizeof(touchData))) {
    data->state = LV_INDEV_STATE_RELEASED;

    return;
  }

  uint8_t touchCount = touchData[0] & 0x0F;

  if (touchCount == 0) {
    data->state = LV_INDEV_STATE_RELEASED;

    return;
  }

  // Reconstruct the 12-bit raw coordinates.
  uint16_t rawX = ((touchData[1] & 0x0F) << 8) | touchData[2];

  uint16_t rawY = ((touchData[3] & 0x0F) << 8) | touchData[4];

  // Reject invalid coordinates.
  if (rawX >= TOUCH_NATIVE_WIDTH || rawY >= TOUCH_NATIVE_HEIGHT) {
    data->state = LV_INDEV_STATE_RELEASED;

    return;
  }

  // ----------------------------------------------------------
  // COORDINATE ROTATION
  // ----------------------------------------------------------
  //
  // Native touchscreen orientation: 320 x 480 (portrait).
  //
  // TFT rotation: 1 (landscape).
  //
  // Convert native FT6336U coordinates to landscape:
  //
  // display X = raw Y
  // display Y = native width - 1 - raw X
  //
  // ----------------------------------------------------------

  uint16_t screenX = rawY;

  uint16_t screenY = (TOUCH_NATIVE_WIDTH - 1) - rawX;

  data->point.x = constrain((int)screenX, 0, SCREEN_WIDTH - 1);

  data->point.y = constrain((int)screenY, 0, SCREEN_HEIGHT - 1);

  data->state = LV_INDEV_STATE_PRESSED;

#endif
}
