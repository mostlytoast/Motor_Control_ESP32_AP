#include "lvdisplay_portable.h"
// todo make this use arduinoJson to simplify code
#include <cstdio>

#include "LvTrackMap.h"
#include "../track/map.h"

TrackMap trackMap;

namespace {

constexpr int kRampStep = 1000;
constexpr int kRampMin = 0;
constexpr int kRampMax = 10000;

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

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return value;
}

void setWidgetText(lv_obj_t* label, const char* text) {
  if (label == nullptr || text == nullptr) {
    return;
  }

  lv_label_set_text(label, text);
}

static void actionHandler(lv_event_t* event) {
  auto* binding = static_cast<ButtonBinding*>(lv_event_get_user_data(event));
  if (binding == nullptr || binding->controller == nullptr) {
    return;
  }

  binding->controller->handleAction(binding->action);
}
static void mapToggleHandler(lv_event_t* event) {
  auto* controller =
      static_cast<LvDisplayController*>(lv_event_get_user_data(event));

  if (controller == nullptr) {
    return;
  }

  controller->toggleTrackMap();
}

}  // namespace

void LvDisplayController::toggleTrackMap() {
  // todo move this toggle map track stuff to lvTrackMap
  if (trackMapContainer_ == nullptr) {
    return;
  }

  mapVisible_ = !mapVisible_;

  if (mapVisible_) {
    lv_obj_set_hidden(trackMapContainer_, false);

    // Map goes over the normal controls.
    lv_obj_move_foreground(trackMapContainer_);

    // Keep the map toggle button above the map.
    if (mapButton_ != nullptr) {
      lv_obj_move_foreground(mapButton_);
    }

    if (mapButton_ != nullptr) {
      lv_obj_t* label = lv_obj_get_child(mapButton_, 0);

      if (label != nullptr) {
        lv_label_set_text(label, "HIDE MAP");
      }
    }
  } else {
    // Hide the map.
    lv_obj_set_hidden(trackMapContainer_, true);

    if (mapButton_ != nullptr) {
      lv_obj_t* label = lv_obj_get_child(mapButton_, 0);

      if (label != nullptr) {
        lv_label_set_text(label, "SHOW MAP");
      }
    }
  }
}
LvDisplayController::LvDisplayController(ILvDisplayHost* host) : host_(host) {}

void LvDisplayController::init(lv_obj_t* screen) {
  screen_ = screen;

  if (screen_ == nullptr) {
    return;
  }

  // LvTrackMap::createTrackMap(screen_);

  // LvTrackMap trackMapDisplay(screen);

  // trackMapDisplay.setTrackMap(trackMap);
  // Create a container for the track map.
  //
  // Keeping the map inside its own container allows the entire map
  // to be shown/hidden without having to individually hide all of
  // the LVGL objects created by LvTrackMap.
  trackMapContainer_ = lv_obj_create(screen_);

  lv_obj_set_size(trackMapContainer_, 320, 240);
  lv_obj_set_pos(trackMapContainer_, 0, 0);

  // Make the container transparent and remove its border/padding.
  lv_obj_set_style_bg_opa(trackMapContainer_, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(trackMapContainer_, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(trackMapContainer_, 0, LV_PART_MAIN);

  // Create the persistent track map inside the container.
  trackMapDisplay_ = new LvTrackMap(trackMapContainer_);

  if (trackMapDisplay_ != nullptr) {
    trackMapDisplay_->setTrackMap(trackMap);
  }

  // Start with the map hidden.
  mapVisible_ = false;
  lv_obj_set_hidden(trackMapContainer_, true);

  lv_obj_set_style_bg_color(screen_, lv_color_hex(kColorBackground),
                            LV_PART_MAIN);

  titleLabel_ = lv_label_create(screen_);
  lv_label_set_text(titleLabel_, "MOTOR CONTROLSSSS");
  lv_obj_set_style_text_color(titleLabel_, lv_color_hex(kColorText),
                              LV_PART_MAIN);
  lv_obj_set_style_text_font(titleLabel_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(titleLabel_, LV_ALIGN_TOP_MID, 0, 2);

  speedLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_color(speedLabel_, lv_color_hex(kColorText),
                              LV_PART_MAIN);
  lv_obj_align(speedLabel_, LV_ALIGN_TOP_LEFT, 8, 53);

  rampLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_color(rampLabel_, lv_color_hex(kColorText),
                              LV_PART_MAIN);
  lv_obj_align(rampLabel_, LV_ALIGN_TOP_MID, 0, 53);

  statusLabel_ = lv_label_create(screen_);
  lv_label_set_text(statusLabel_, "STOPPED");
  lv_obj_set_style_text_color(statusLabel_, lv_color_hex(kColorStatus),
                              LV_PART_MAIN);
  lv_obj_align(statusLabel_, LV_ALIGN_TOP_RIGHT, -8, 53);

  forwardButton_ = createActionButton(screen_, "FORWARD", 5, 78, 100, 45,
                                      LvDisplayAction::Forward, kColorForward);
  stopButton_ = createActionButton(screen_, "STOP", 110, 78, 100, 45,
                                   LvDisplayAction::Stop, kColorStop);
  reverseButton_ = createActionButton(screen_, "REVERSE", 215, 78, 100, 45,
                                      LvDisplayAction::Reverse, kColorReverse);

  rampDownButton_ =
      createActionButton(screen_, "RAMP -", 5, 128, 100, 40,
                         LvDisplayAction::RampDown, kColorButtonOff);
  rampUpButton_ = createActionButton(screen_, "RAMP +", 110, 128, 100, 40,
                                     LvDisplayAction::RampUp, kColorButtonOff);
  led1Button_ = createActionButton(screen_, "LED 1", 215, 128, 100, 40,
                                   LvDisplayAction::Led1, kColorButtonOff);

  led2Button_ = createActionButton(screen_, "LED 2", 5, 173, 100, 40,
                                   LvDisplayAction::Led2, kColorButtonOff);
  autoButton_ = createActionButton(screen_, "AUTO", 110, 173, 100, 40,
                                   LvDisplayAction::Auto, kColorButtonOff);
  audioButton_ = createActionButton(screen_, "AUDIO", 215, 173, 100, 40,
                                    LvDisplayAction::Audio, kColorButtonOff);

  mapButton_ = lv_button_create(screen_);

  lv_obj_set_pos(mapButton_, 5, 27);
  lv_obj_set_size(mapButton_, 100, 22);

  lv_obj_set_style_radius(mapButton_, 6, LV_PART_MAIN);

  lv_obj_set_style_bg_color(mapButton_, lv_color_hex(KColorMenuHeader),
                            LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_color(mapButton_, lv_color_hex(kColorPressed),
                            LV_PART_MAIN | LV_STATE_PRESSED);

  lv_obj_t* mapLabel = lv_label_create(mapButton_);

  lv_label_set_text(mapLabel, "SHOW MAP");

  lv_obj_set_style_text_color(mapLabel, lv_color_hex(kColorText), LV_PART_MAIN);

  lv_obj_center(mapLabel);

  lv_obj_add_event_cb(mapButton_, mapToggleHandler, LV_EVENT_CLICKED, this);

  setButtonState(led1Button_, false);
  setButtonState(led2Button_, false);
  setButtonState(autoButton_, false);
  setButtonState(audioButton_, false);

  lv_obj_t* info = lv_label_create(screen_);
  lv_label_set_text(info, "ESP-NOW");
  lv_obj_set_style_text_color(info, lv_color_hex(kColorInfo), LV_PART_MAIN);
  lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -3);

  applyStateToUi();
}

void LvDisplayController::refresh() { applyStateToUi(); }

void LvDisplayController::setMotorSpeed(int value) {
  state_.motorSpeed = clampInt(value, -255, 255);
  updateSpeedLabel();
  updateStatusLabel();
}

void LvDisplayController::setRampSetting(int value) {
  state_.rampSetting = clampInt(value, kRampMin, kRampMax);
  updateRampLabel();
}

void LvDisplayController::setLed1(bool enabled) {
  state_.led1State = enabled;
  setButtonState(led1Button_, enabled);
  updateStatusLabel();
}

void LvDisplayController::setLed2(bool enabled) {
  state_.led2State = enabled;
  setButtonState(led2Button_, enabled);
  updateStatusLabel();
}

void LvDisplayController::setAuto(bool enabled) {
  state_.autoState = enabled;
  setButtonState(autoButton_, enabled);
  updateStatusLabel();
}

void LvDisplayController::setAudio(bool enabled) {
  state_.audioState = enabled;
  setButtonState(audioButton_, enabled);
  updateStatusLabel();
}

void LvDisplayController::handleAction(LvDisplayAction action) {
  switch (action) {
    case LvDisplayAction::Forward:

      state_.motorSpeed = clampInt(state_.motorSpeed + 10, -255, 255);

      break;

    case LvDisplayAction::Reverse:

      state_.motorSpeed = clampInt(state_.motorSpeed - 10, -255, 255);

      break;

    case LvDisplayAction::Stop:

      state_.motorSpeed = 0;

      break;

    case LvDisplayAction::RampUp:

      state_.rampSetting =
          clampInt(state_.rampSetting + kRampStep, kRampMin, kRampMax);

      break;

    case LvDisplayAction::RampDown:

      state_.rampSetting =
          clampInt(state_.rampSetting - kRampStep, kRampMin, kRampMax);

      break;

    case LvDisplayAction::Led1:

      state_.led1State = !state_.led1State;

      break;

    case LvDisplayAction::Led2:

      state_.led2State = !state_.led2State;

      break;

    case LvDisplayAction::Auto:

      state_.autoState = !state_.autoState;

      break;

    case LvDisplayAction::Audio:

      state_.audioState = !state_.audioState;

      break;
  }

  // Update the physical/backend side with the complete state.
  sendStateJson();

  // Update LVGL immediately.
  applyStateToUi();
}
void LvDisplayController::setButtonState(lv_obj_t* button, bool state) {
  if (button == nullptr) {
    return;
  }

  lv_color_t color =
      state ? lv_color_hex(kColorButtonOn) : lv_color_hex(kColorButtonOff);

  lv_obj_set_style_bg_color(button, color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(button, color, LV_PART_MAIN | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(button, lv_color_hex(kColorPressed),
                            LV_PART_MAIN | LV_STATE_PRESSED);
}

void LvDisplayController::updateSpeedLabel() {
  if (speedLabel_ == nullptr) {
    return;
  }

  char text[32];
  std::snprintf(text, sizeof(text), "Speed: %d", abs(state_.motorSpeed));
  setWidgetText(speedLabel_, text);
}

void LvDisplayController::updateRampLabel() {
  if (rampLabel_ == nullptr) {
    return;
  }

  char text[32];
  std::snprintf(text, sizeof(text), "Ramp: %d", state_.rampSetting / 1000);
  setWidgetText(rampLabel_, text);
}

void LvDisplayController::updateStatusLabel() {
  if (statusLabel_ == nullptr) {
    return;
  }

  if (state_.autoState) {
    setWidgetText(statusLabel_, "AUTO ON");
    return;
  }

  if (state_.audioState) {
    setWidgetText(statusLabel_, "AUDIO ON");
    return;
  }

  if (state_.led1State) {
    setWidgetText(statusLabel_, "LED 1 ON");
    return;
  }

  if (state_.led2State) {
    setWidgetText(statusLabel_, "LED 2 ON");
    return;
  }

  if (state_.motorSpeed > 0) {
    setWidgetText(statusLabel_, "FORWARD");
    return;
  }

  if (state_.motorSpeed < 0) {
    setWidgetText(statusLabel_, "REVERSE");
    return;
  }

  setWidgetText(statusLabel_, "STOPPED");
}

bool findJsonValue(const char* json, const char* key, const char** valueStart) {
  if (json == nullptr || key == nullptr || valueStart == nullptr) {
    return false;
  }

  char searchKey[64];

  std::snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

  const char* keyPosition = strstr(json, searchKey);

  if (keyPosition == nullptr) {
    return false;
  }

  const char* colon = strchr(keyPosition + strlen(searchKey), ':');

  if (colon == nullptr) {
    return false;
  }

  const char* value = colon + 1;

  while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {
    ++value;
  }

  *valueStart = value;

  return true;
}

bool getJsonBool(const char* json, const char* key, bool& output) {
  const char* value = nullptr;

  if (!findJsonValue(json, key, &value)) {
    return false;
  }

  if (strncmp(value, "true", 4) == 0) {
    output = true;
    return true;
  }

  if (strncmp(value, "false", 5) == 0) {
    output = false;
    return true;
  }

  // Also accept 1 / 0.
  if (*value == '1') {
    output = true;
    return true;
  }

  if (*value == '0') {
    output = false;
    return true;
  }

  return false;
}

bool getJsonInt(const char* json, const char* key, int& output) {
  const char* value = nullptr;

  if (!findJsonValue(json, key, &value)) {
    return false;
  }

  char* end = nullptr;

  long parsed = strtol(value, &end, 10);

  if (end == value) {
    return false;
  }

  output = static_cast<int>(parsed);

  return true;
}

void LvDisplayController::getStateFromJson(const char* json) {
  std::printf("[LVGL JSON] ===== START getStateFromJson =====\n");

  if (json == nullptr) {
    std::printf("[LVGL JSON] ERROR: json == nullptr\n");
    return;
  }

  std::printf("[LVGL JSON] Received state: %s\n", json);

  std::printf("[LVGL JSON] Testing led1...\n");

  bool led1 = false;
  bool resultLed1 = getJsonBool(json, "led1", led1);

  std::printf("[LVGL JSON] led1 result=%d value=%d\n", resultLed1, led1);

  std::printf("[LVGL JSON] Testing led2...\n");

  bool led2 = false;
  bool resultLed2 = getJsonBool(json, "led2", led2);

  std::printf("[LVGL JSON] led2 result=%d value=%d\n", resultLed2, led2);

  std::printf("[LVGL JSON] Testing auto...\n");

  bool autoState = false;
  bool resultAuto = getJsonBool(json, "auto", autoState);

  std::printf("[LVGL JSON] auto result=%d value=%d\n", resultAuto, autoState);

  std::printf("[LVGL JSON] Testing speed...\n");

  int speed = 0;
  bool resultSpeed = getJsonInt(json, "speed", speed);

  std::printf("[LVGL JSON] speed result=%d value=%d\n", resultSpeed, speed);

  std::printf("[LVGL JSON] Testing ramp...\n");

  int ramp = 0;
  bool resultRamp = getJsonInt(json, "ramp", ramp);

  std::printf("[LVGL JSON] ramp result=%d value=%d\n", resultRamp, ramp);

  std::printf("[LVGL JSON] Testing audio...\n");

  bool audio = false;
  bool resultAudio = getJsonBool(json, "audio", audio);

  std::printf("[LVGL JSON] audio result=%d value=%d\n", resultAudio, audio);

  std::printf("[LVGL JSON] All fields parsed\n");

  // Apply the values.
  state_.led1State = led1;
  state_.led2State = led2;
  state_.autoState = autoState;
  state_.audioState = audio;

  state_.motorSpeed = clampInt(speed, -255, 255);

  state_.rampSetting = clampInt(ramp, kRampMin, kRampMax);

  std::printf("[LVGL JSON] State assigned\n");

  std::printf(
      "[LVGL JSON] Parsed state:\n"
      "  led1  = %s\n"
      "  led2  = %s\n"
      "  auto  = %s\n"
      "  speed = %d\n"
      "  ramp  = %d\n"
      "  audio = %s\n",
      state_.led1State ? "true" : "false", state_.led2State ? "true" : "false",
      state_.autoState ? "true" : "false", state_.motorSpeed,
      state_.rampSetting / 1000, state_.audioState ? "true" : "false");

  std::printf("[LVGL JSON] Calling applyStateToUi()\n");

  applyStateToUi();

  std::printf("[LVGL JSON] applyStateToUi() returned\n");

  std::printf("[LVGL JSON] ===== END getStateFromJson =====\n");
}

void LvDisplayController::sendStateJson() {
  if (host_ == nullptr) {
    return;
  }

  char json[256];

  std::snprintf(json, sizeof(json),

                "{"
                "\"led1\":%s,"
                "\"led2\":%s,"
                "\"auto\":%s,"
                "\"speed\":%d,"
                "\"ramp\":%d,"
                "\"audio\":%s"
                "}",

                state_.led1State ? "true" : "false",
                state_.led2State ? "true" : "false",
                state_.autoState ? "true" : "false",

                state_.motorSpeed,

                // Backend expects seconds.
                state_.rampSetting / 1000,

                state_.audioState ? "true" : "false");

  host_->sendJson(json);
}
void LvDisplayController::applyStateToUi() {
  updateSpeedLabel();
  updateRampLabel();
  updateStatusLabel();

  setButtonState(led1Button_, state_.led1State);
  setButtonState(led2Button_, state_.led2State);
  setButtonState(autoButton_, state_.autoState);
  setButtonState(audioButton_, state_.audioState);
}

lv_obj_t* LvDisplayController::createActionButton(lv_obj_t* parent,
                                                  const char* text, int x,
                                                  int y, int width, int height,
                                                  LvDisplayAction action,
                                                  uint32_t normalColor) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);

  lv_obj_set_style_bg_color(button, lv_color_hex(normalColor),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(button, lv_color_hex(kColorPressed),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(button, lv_color_hex(normalColor),
                            LV_PART_MAIN | LV_STATE_CHECKED);

  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(kColorText), LV_PART_MAIN);
  lv_obj_center(label);

  ButtonBinding binding{action, this};
  bindings_.push_back(std::make_unique<ButtonBinding>(binding));
  lv_obj_add_event_cb(button, actionHandler, LV_EVENT_CLICKED,
                      bindings_.back().get());

  return button;
}
