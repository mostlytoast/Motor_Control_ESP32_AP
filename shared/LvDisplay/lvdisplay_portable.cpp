#include "lvdisplay_portable.h"
// todo make this use arduinoJson to simplify code

TrackMap trackMap;

namespace {

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

static void mapToggleHandler(lv_event_t* event) {
  auto* controller =
      static_cast<LvDisplayController*>(lv_event_get_user_data(event));

  if (controller == nullptr) {
    return;
  }

  controller->toggleTrackMap();
}

}  // namespace

LvDisplayController::LvDisplayController(ILvDisplayHost* host) : host_(host) {}

void LvDisplayController::init(lv_obj_t* screen) {
  screen_ = screen;

  if (screen_ == nullptr) {
    return;
  }

  // LvTrackMap::createTrackMap(screen_);

  // LvTrackMap trackMapDisplay(screen);

  // trackMapDisplay_.setTrackMap(trackMap);
  // Create a container for the track map.
  //
  // Keeping the map inside its own container allows the entire map
  // to be shown/hidden without having to individually hide all of
  // the LVGL objects created by LvTrackMap.
  // trackMapContainer_ = lv_obj_create(screen_);

  // lv_obj_set_size(trackMapContainer_, 320, 240);
  // lv_obj_set_pos(trackMapContainer_, 0, 0);

  // // Make the container transparent and remove its border/padding.
  // lv_obj_set_style_bg_opa(trackMapContainer_, LV_OPA_TRANSP, LV_PART_MAIN);
  // lv_obj_set_style_border_width(trackMapContainer_, 0, LV_PART_MAIN);
  // lv_obj_set_style_pad_all(trackMapContainer_, 0, LV_PART_MAIN);

  // Create the persistent track map inside the container.
  trackMapDisplay_ = new LvTrackMap(screen_);

  if (trackMapDisplay_ != nullptr) {
    trackMapDisplay_->setTrackMap(trackMap);
  }

  // Start with the map hidden.
  mapVisible_ = false;
  // lv_obj_set_hidden(trackMapContainer_, true);

  lv_obj_set_style_bg_color(screen_, lv_color_hex(kColorBackground),
                            LV_PART_MAIN);

  titleLabel_ = lv_label_create(screen_);
  lv_label_set_text(titleLabel_, "MOTOR CONTROLS V6");
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

  forwardButton_ = display_.createActionButton(
      screen_, "FORWARD", 5, 78, 100, 45,
      [this]() {
        app.setUserSpeed(clampInt(app.getUserSpeed() + 10, -255, 255));
        sendStateToReceiver();
      },
      kColorForward);
  stopButton_ = display_.createActionButton(
      screen_, "STOP", 110, 78, 100, 45,
      [this]() {
        app.setUserSpeed(0);
        sendStateToReceiver();
      },
      kColorStop);
  reverseButton_ = display_.createActionButton(
      screen_, "REVERSE", 215, 78, 100, 45,
      [this]() {
        app.setUserSpeed(clampInt(app.getUserSpeed() - 10, -255, 255));
        sendStateToReceiver();
      },
      kColorReverse);

  rampDownButton_ = display_.createActionButton(
      screen_, "RAMP -", 5, 128, 100, 40,
      [this]() {
        app.setRampTime(
            clampInt(app.getRampTime() - kRampStep, kRampMin, kRampMax));
        sendStateToReceiver();
      },
      kColorButtonOff);
  rampUpButton_ = display_.createActionButton(
      screen_, "RAMP +", 110, 128, 100, 40,
      [this]() {
        app.setRampTime(
            clampInt(app.getRampTime() + kRampStep, kRampMin, kRampMax));
        sendStateToReceiver();
      },
      kColorButtonOff);
  led1Button_ = display_.createActionButton(
      screen_, "LED 1", 215, 128, 100, 40,
      [this]() {
        app.setLed1State(!app.getLed1State());
        sendStateToReceiver();
      },
      kColorButtonOff);

  led2Button_ = display_.createActionButton(
      screen_, "LED 2", 5, 173, 100, 40,
      [this]() {
        app.setLed2State(!app.getLed2State());
        sendStateToReceiver();
      },
      kColorButtonOff);
  autoButton_ = display_.createActionButton(
      screen_, "AUTO", 110, 173, 100, 40,
      [this]() {
        app.setAutoState(!app.getAutoState());
        sendStateToReceiver();
      },
      kColorButtonOff);
  audioButton_ = display_.createActionButton(
      screen_, "AUDIO", 215, 173, 100, 40,
      [this]() {
        app.setAudioState(!app.getAudioState());
        sendStateToReceiver();
      },
      kColorButtonOff);
  mapButton_ = display_.createActionButton(
      screen_, "show Map", 5, 27, 100, 22,
      [this]() { trackMapDisplay_->toggle(); }, KColorMenuHeader);

  trackScannerButton = display_.createActionButton(
      screen_, "SCANNER", 110, 27, 100, 25,
      [this]() { trackScanningDisplay_->toggle(); }, COLOR_MENU_HEADER);

  trackScanningDisplay_ = new TrackScanningDisplay(screen_);

  lv_obj_t* info = lv_label_create(screen_);
  lv_label_set_text(info, "ESP-NOW");
  lv_obj_set_style_text_color(info, lv_color_hex(kColorInfo), LV_PART_MAIN);
  lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -3);

  applyStateToUi();
}

void LvDisplayController::refresh() {
  applyStateToUi();
  if (trackMapDisplay_ != nullptr) {
    trackMapDisplay_->refresh();
  }
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
  std::snprintf(text, sizeof(text), "Ramp: %lu", app.getRampTime() / 1000);
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

// TODO check this is working on both sides

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
  // todo check if refeshing too much
  refresh();
}

void LvDisplayController::sendStateToReceiver() {
  JsonDocument outputDoc;
  getJsonFromState(outputDoc);
  String outputString;
  serializeJson(outputDoc, outputString);
  // Serial.println("outputString");
  // Serial.println(outputString);
  host_->sendJson(outputString.c_str());

  applyStateToUi();
}

void LvDisplayController::applyStateToUi() {
  updateSpeedLabel();
  updateRampLabel();
  updateStatusLabel();

  display_.setButtonState(led1Button_, app.getLed1State());
  display_.setButtonState(led2Button_, app.getLed2State());
  display_.setButtonState(autoButton_, app.getAutoState());
  display_.setButtonState(audioButton_, app.getAudioState());
}
