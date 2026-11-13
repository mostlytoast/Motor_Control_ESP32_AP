#include "lvdisplay_portable.h"
// todo make this use arduinoJson to simplify code
namespace {
JsonDocument previousState;
bool previousStateUpdated = false;
}  // namespace
TrackMap trackMap;

LvDisplayController::LvDisplayController(ILvDisplayHost* host) : host_(host) {}

void LvDisplayController::init(lv_obj_t* screen) {
  screen_ = screen;

  if (screen_ == nullptr) {
    return;
  }

  // Create the persistent track map inside the container.
  trackMapDisplay_ =
      new LvTrackMap(screen_, [this]() { this->sendStateToReceiver(); });

  if (trackMapDisplay_ != nullptr) {
    trackMapDisplay_->setTrackMap(trackMap);
  }

  lv_obj_set_style_bg_color(screen_, lv_color_hex(kColorBackground),
                            LV_PART_MAIN);

  titleLabel_ = display_.createLabel(screen_, kColorText, 0, 2, LV_PART_MAIN,
                                     LV_ALIGN_TOP_MID, "MOTOR CONTROLS V7");

  speedLabel_ = display_.createLabel(screen_, kColorText, 8, 53, LV_PART_MAIN,
                                     LV_ALIGN_TOP_LEFT, "Speed: 0");

  rampLabel_ = display_.createLabel(screen_, kColorText, 0, 53, LV_PART_MAIN,
                                    LV_ALIGN_TOP_MID, "Ramp: 0");

  // TODO make this show on top of everything
  statusLabel_ =
      display_.createLabel(screen_, kColorStatus, -8, 53, LV_PART_MAIN,
                           LV_ALIGN_TOP_RIGHT, "No Peer");

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
        app.setStatusLabelText("");
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

  trackScanningDisplay_ = new TrackScanningDisplay(
      screen_, [this]() { this->sendStateToReceiver(); });

  lv_obj_t* info = display_.createLabel(
      screen_, kColorInfo, 0, -3, LV_PART_MAIN, LV_ALIGN_BOTTOM_MID, "ESP-NOW");

  lv_label_set_text(info, "ESP-NOW");
  lv_obj_set_style_text_color(info, lv_color_hex(kColorInfo), LV_PART_MAIN);
  lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -3);

  applyStateToUi();
}
void LvDisplayController::reset() {
  if (trackMapDisplay_ != nullptr) {
    trackMapDisplay_->reset();
  }
  // if (trackScanningDisplay_ != nullptr) {
  //   trackScanningDisplay_->reset();
  // }
  refresh();
}

void LvDisplayController::refresh() {
  // Serial.println("port display refresh");

  if (trackMapDisplay_ != nullptr) {
    applyStateToUi();
    trackMapDisplay_->refresh();
  }
  if (trackScanningDisplay_ != nullptr) {
    trackScanningDisplay_->refresh();
  }
}

void LvDisplayController::updateSpeedLabel() {
  if (speedLabel_ == nullptr) {
    Serial.println("speed label is null");
    return;
  }

  char text[32];
  std::snprintf(text, sizeof(text), "Speed: %d", abs(app.getUserSpeed()));
  display_.setLabelText(speedLabel_, text);
}

void LvDisplayController::updateRampLabel() {
  if (rampLabel_ == nullptr) {
    Serial.println("ramp label is null");

    return;
  }

  char text[32];
  std::snprintf(text, sizeof(text), "Ramp: %lu", app.getRampTime() / 1000);
  display_.setLabelText(rampLabel_, text);
}

void LvDisplayController::updateStatusLabel() {
  if (statusLabel_ == nullptr) {
    return;
  }
  if (!app.getStatusLabelText().equals("")) {
    display_.setLabelText(statusLabel_, app.getStatusLabelText().c_str());
    return;
  }
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
  getStateFromCondensedJson(doc);
  // todo check if refreshing too much
  refresh();
}

void LvDisplayController::sendStateToReceiver() {
  // Serial.println("called sendStateToReceiver");
  JsonDocument state;

  getCondensedJsonFromState(state);
  if (state.isNull()) {
    return;
  }

  if (!previousStateUpdated) {
    previousState = state;  // Ensure previousState is a JsonDocument type
    previousStateUpdated = true;
    return;  // Return  because nothing was sent yet
  }

  if (previousState == state) {
    return;
  }

  previousState = state;

  String outputString;
  serializeJson(state, outputString);
  host_->sendJson(outputString.c_str());
  // Serial.println("state sent");

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
