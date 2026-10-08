#include "lvdisplay_portable.h"
// todo make this use arduinoJson to simplify code
#include <cstdio>

#include "../track/map.h"
#include "LvTrackMap.h"

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
    Serial.println("SHOWING MAP");
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

    if (trackMapDisplay_ != nullptr) {
      trackMapDisplay_->refresh();
    }
  } else {
    Serial.println("HIDING MAP");
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
  lv_label_set_text(titleLabel_, "MOTOR CONTROLS3");
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

void LvDisplayController::refresh() {
  applyStateToUi();
  trackMapDisplay_->refresh();
}

void LvDisplayController::setMotorSpeed(int value) {
  app.setUserSpeed(clampInt(value, -255, 255));
  updateSpeedLabel();
  updateStatusLabel();
}

void LvDisplayController::setRampSetting(int value) {
  app.setRampTime(clampInt(value, kRampMin, kRampMax));
  updateRampLabel();
}

void LvDisplayController::setLed1(bool enabled) {
  app.setLed1State(enabled);
  setButtonState(led1Button_, enabled);
  updateStatusLabel();
}

void LvDisplayController::setLed2(bool enabled) {
  app.setLed2State(enabled);
  setButtonState(led2Button_, enabled);
  updateStatusLabel();
}

void LvDisplayController::setAuto(bool enabled) {
  app.setAutoState(enabled);
  setButtonState(autoButton_, enabled);
  updateStatusLabel();
}

void LvDisplayController::setAudio(bool enabled) {
  app.setAudioState(enabled);
  setButtonState(audioButton_, enabled);
  updateStatusLabel();
}

void LvDisplayController::handleAction(LvDisplayAction action) {
  switch (action) {
    case LvDisplayAction::Forward:

      app.setUserSpeed(clampInt(app.getUserSpeed() + 10, -255, 255));

      break;

    case LvDisplayAction::Reverse:

      app.setUserSpeed(clampInt(app.getUserSpeed() - 10, -255, 255));

      break;

    case LvDisplayAction::Stop:

      app.setUserSpeed(0);

      break;

    case LvDisplayAction::RampUp:

      app.setRampTime(
          clampInt(app.getRampTime() + kRampStep, kRampMin, kRampMax));

      break;

    case LvDisplayAction::RampDown:

      app.setRampTime(
          clampInt(app.getRampTime() - kRampStep, kRampMin, kRampMax));

      break;

    case LvDisplayAction::Led1:

      app.setLed1State(!app.getLed1State());

      break;

    case LvDisplayAction::Led2:

      app.setLed2State(!app.getLed2State());

      break;

    case LvDisplayAction::Auto:

      app.setAutoState(!app.getAutoState());

      break;

    case LvDisplayAction::Audio:

      app.setAudioState(!app.getAudioState());

      break;
  }

  // Update the physical/backend side with the complete state.
  JsonDocument outputDoc;
  getJsonFromState(outputDoc);
  String outputString;
  serializeJson(outputDoc, outputString);
  Serial.println("outputString");
  Serial.println(outputString);
  host_->sendJson(outputString.c_str());

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
  std::snprintf(text, sizeof(text), "Speed: %d", abs(app.getUserSpeed()));
  setWidgetText(speedLabel_, text);
}

void LvDisplayController::updateRampLabel() {
  if (rampLabel_ == nullptr) {
    return;
  }

  char text[32];
  std::snprintf(text, sizeof(text), "Ramp: %d", app.getRampTime() / 1000);
  setWidgetText(rampLabel_, text);
}

void LvDisplayController::updateStatusLabel() {
  if (statusLabel_ == nullptr) {
    return;
  }

  if (app.getAutoState()) {
    setWidgetText(statusLabel_, "AUTO ON");
    return;
  }

  if (app.getAudioState()) {
    setWidgetText(statusLabel_, "AUDIO ON");
    return;
  }

  if (app.getLed1State()) {
    setWidgetText(statusLabel_, "LED 1 ON");
    return;
  }

  if (app.getLed2State()) {
    setWidgetText(statusLabel_, "LED 2 ON");
    return;
  }

  if (app.getUserSpeed() > 0) {
    setWidgetText(statusLabel_, "FORWARD");
    return;
  }

  if (app.getUserSpeed() < 0) {
    setWidgetText(statusLabel_, "REVERSE");
    return;
  }

  setWidgetText(statusLabel_, "STOPPED");
}

void LvDisplayController::getStateFromJsonDisplay(const char* json) {
  JsonDocument doc;

  // Parse the JSON string
  DeserializationError error = deserializeJson(doc, json);

  // Check if parsing succeeded
  if (error) {
    Serial.print("Parsing failed: ");
    Serial.println(error.c_str());
    return;
  }
  getStateFromJson(doc);
  refresh();
  
}

void LvDisplayController::applyStateToUi() {
  updateSpeedLabel();
  updateRampLabel();
  updateStatusLabel();

  setButtonState(led1Button_, app.getLed1State());
  setButtonState(led2Button_, app.getLed2State());
  setButtonState(autoButton_, app.getAutoState());
  setButtonState(audioButton_, app.getAudioState());
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
