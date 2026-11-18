#include "lvdisplay_portable.h"
// todo make this use arduinoJson to simplify code
// TODO odd web bug where buttons dont initially respond until multiple are
// clicked?
namespace {
JsonDocument previousState;
bool previousStateUpdated = false;
}  // namespace
TrackMap trackMap;

LvDisplayController::LvDisplayController(ILvDisplayHost* host) : host_(host) {}

lv_obj_t* LvDisplayController::init(lv_obj_t* screen) {
  Serial.println("[DISPLAY] init started");

  screen_ = screen;

  if (!screen_) {
    Serial.println("[DISPLAY] ERROR: screen is null");
    return nullptr;
  }

  Serial.printf("[DISPLAY] Screen: %p\n", screen_);

  trackMapDisplay_ =
      new LvTrackMap(screen_, [this]() { this->sendStateToReceiver(); });

  Serial.println("[DISPLAY] Track map created");

  trackScanningDisplay_ = new TrackScanningDisplay(
      screen_, [this]() { this->sendStateToReceiver(); });
  lv_obj_set_scrollable(screen_, false);

  // lv_obj_set_style_bg_color(screen_,
  // lv_color_hex(colorConfig.backgroundColor),
  //                           LV_PART_MAIN);
  Serial.println("[DISPLAY] Scanner created");

  grid_cont = lv_obj_create(screen_);

  Serial.printf("[DISPLAY] Grid created: %p\n", grid_cont);

  lv_obj_set_width(grid_cont, lv_pct(100));
  lv_obj_set_height(grid_cont, lv_pct(100));

  /* Center the container on the screen */
  lv_obj_center(grid_cont);
  lv_obj_set_style_bg_opa(grid_cont, LV_OPA_TRANSP, LV_PART_MAIN);

  // lv_obj_set_style_bg_color(
  //     grid_cont, lv_color_hex(colorConfig.backgroundColor), LV_PART_MAIN);
  static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_TEMPLATE_LAST};

  /* Define 2 rows: equal height */
  static int32_t row_dsc[] = {
      LV_GRID_FR(2), LV_GRID_FR(4), LV_GRID_FR(4), LV_GRID_FR(6),
      LV_GRID_FR(6), LV_GRID_FR(6), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(grid_cont, col_dsc, row_dsc);
  lv_obj_set_layout(grid_cont, LV_LAYOUT_GRID);

  titleLabel_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.textColor), 0, 2, LV_PART_MAIN,
      LV_ALIGN_CENTER, "MOTOR CONTROLS V8");

  lv_obj_set_grid_cell(titleLabel_, LV_GRID_ALIGN_CENTER, 1, 0,
                       LV_GRID_ALIGN_CENTER, 0, 0);

  mapButton_ = sharedDisplay.createActionButton(
      grid_cont, "show Map",
      [this](lv_event_t* event) {
        trackMapDisplay_->toggle();
        // lv_obj_move_foreground(this->statusLabel_);
      },
      lv_color_hex(colorConfig.menuHeaderColor));
  lv_obj_set_grid_cell(mapButton_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);

  trackScannerButton = sharedDisplay.createActionButton(
      grid_cont, "SCANNER",
      [this](lv_event_t* event) {
        trackScanningDisplay_->toggle();
        // lv_obj_move_foreground(this->statusLabel_);
      },
      lv_color_hex(colorConfig.menuHeaderColor));
  lv_obj_set_grid_cell(trackScannerButton, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);

  speedLabel_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.textColor), 8, 53, LV_PART_MAIN,
      LV_ALIGN_TOP_LEFT, app.getUserSpeedLabelLambda);
  lv_obj_set_grid_cell(speedLabel_, LV_GRID_ALIGN_CENTER, 0, 1,
                       LV_GRID_ALIGN_CENTER, 2, 1);

  rampLabel_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.textColor), 0, 53, LV_PART_MAIN,
      LV_ALIGN_TOP_MID, app.getRampTimeLabelLambda);

  lv_obj_set_grid_cell(rampLabel_, LV_GRID_ALIGN_CENTER, 1, 1,
                       LV_GRID_ALIGN_CENTER, 2, 1);
  // TODO make this show on top of everything
  // lv_obj_t * top_obj = lv_obj_create(lv_layer_top());

  // lv_obj_set_grid_cell(statusLabel_, LV_GRID_ALIGN_STRETCH, 2, 1,
  //                      LV_GRID_ALIGN_STRETCH, 1, 1);
  forwardButton_ = sharedDisplay.createActionButton(
      grid_cont, "FORWARD",
      [this](lv_event_t* event) {
        // TODO use the min and max for the lambda
        app.setUserSpeed(clampInt(app.getUserSpeed() + 10, -255, 255));
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.forwardColor));

  lv_obj_set_grid_cell(forwardButton_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);
  stopButton_ = sharedDisplay.createActionButton(
      grid_cont, "STOP",
      [this](lv_event_t* event) {
        app.setUserSpeed(0);
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.stopColor));
  lv_obj_set_grid_cell(stopButton_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);
  reverseButton_ = sharedDisplay.createActionButton(
      grid_cont, "REVERSE",
      [this](lv_event_t* event) {
        app.setUserSpeed(clampInt(app.getUserSpeed() - 10, -255, 255));
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.reverseColor));
  lv_obj_set_grid_cell(reverseButton_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);
  rampDownButton_ = sharedDisplay.createActionButton(
      grid_cont, "RAMP -",
      [this](lv_event_t* event) {
        app.setRampTime(
            clampInt(app.getRampTime() - kRampStep, kRampMin, kRampMax));
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.inactiveColor));

  lv_obj_set_grid_cell(rampDownButton_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 4, 1);
  rampUpButton_ = sharedDisplay.createActionButton(
      grid_cont, "RAMP +",
      [this](lv_event_t* event) {
        app.setRampTime(
            clampInt(app.getRampTime() + kRampStep, kRampMin, kRampMax));
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.inactiveColor));
  lv_obj_set_grid_cell(rampUpButton_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 4, 1);
  led1Button_ = sharedDisplay.createActionButton(
      grid_cont, "LED 1",
      [this](lv_event_t* event) {
        app.setLed1State(!app.getLed1State());
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.inactiveColor),
      [this]() { return app.getLed1State(); });
  lv_obj_set_grid_cell(led1Button_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 4, 1);

  led2Button_ = sharedDisplay.createActionButton(
      grid_cont, "LED 2",
      [this](lv_event_t* event) {
        app.setLed2State(!app.getLed2State());
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.inactiveColor),
      [this]() { return app.getLed2State(); });

  lv_obj_set_grid_cell(led2Button_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 5, 1);
  autoButton_ = sharedDisplay.createActionButton(
      grid_cont, "AUTO",
      [this](lv_event_t* event) {
        app.setAutoState(!app.getAutoState());
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.inactiveColor),
      [this]() { return app.getAutoState(); });
  lv_obj_set_grid_cell(autoButton_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 5, 1);
  audioButton_ = sharedDisplay.createActionButton(
      grid_cont, "AUDIO",
      [this](lv_event_t* event) {
        app.setAmbianceAudioMode(!app.getAmbianceAudioMode());
        sendStateToReceiver();
      },
      lv_color_hex(colorConfig.inactiveColor),
      [this]() { return app.getAmbianceAudioMode(); });
  lv_obj_set_grid_cell(audioButton_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 5, 1);

  infoLabel_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.infoColor), 0, -3, LV_PART_MAIN,
      LV_ALIGN_CENTER, "ESP-NOW");
  lv_obj_set_grid_cell(infoLabel_, LV_GRID_ALIGN_CENTER, 1, 1,
                       LV_GRID_ALIGN_CENTER, 6, 1);

  // lv_obj_move_to_index(grid_cont, 1);
  applyStateToUi();
  return grid_cont;
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

  applyStateToUi();
  if (trackMapDisplay_ != nullptr) {
    trackMapDisplay_->refresh();
  }
  if (trackScanningDisplay_ != nullptr) {
    trackScanningDisplay_->refresh();
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
  // Serial.println("called sendStateToReceiver portable");
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
  // Serial.println("refreshAndSend");

  previousState = state;

  String outputString;
  serializeJson(state, outputString);
  host_->sendJson(outputString.c_str());
  // Serial.println("state sent");

  applyStateToUi();
}

void LvDisplayController::applyStateToUi() { sharedDisplay.updateStates(); }
