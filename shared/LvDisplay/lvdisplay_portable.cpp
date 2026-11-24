#include "lvdisplay_portable.h"

// TODO odd web bug where buttons dont initially respond until multiple are
// clicked?

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

  //  ============================= TITLE LABEL =============================
  titleLabel_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.textColor), 0, 2, LV_PART_MAIN,
      LV_ALIGN_CENTER, "MOTOR CONTROLS V8");

  lv_obj_set_grid_cell(titleLabel_, LV_GRID_ALIGN_CENTER, 1, 0,
                       LV_GRID_ALIGN_CENTER, 0, 0);

  //  ============================= MAP =============================
  mapButton_ = sharedDisplay.createActionButton(
      grid_cont, "show Map",
      [this]() {
        trackMapDisplay_->toggle();
        // lv_obj_move_foreground(this->statusLabel_);
      },
      lv_color_hex(colorConfig.menuHeaderColor));
  lv_obj_set_grid_cell(mapButton_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);

  //  ============================= TRACK SCANNER =============================
  trackScannerButton = sharedDisplay.createActionButton(
      grid_cont, "SCANNER",
      [this]() {
        trackScanningDisplay_->toggle();
        // lv_obj_move_foreground(this->statusLabel_);
      },
      lv_color_hex(colorConfig.menuHeaderColor));
  lv_obj_set_grid_cell(trackScannerButton, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);

  //  ============================= SPEED LABEL =============================
  speedLabel_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.textColor), 8, 53, LV_PART_MAIN,
      LV_ALIGN_TOP_LEFT, app.getUserSpeedLabelLambda);
  lv_obj_set_grid_cell(speedLabel_, LV_GRID_ALIGN_CENTER, 0, 1,
                       LV_GRID_ALIGN_CENTER, 2, 1);

  //  ============================= RAMP LABEL =============================
  rampLabel_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.textColor), 0, 53, LV_PART_MAIN,
      LV_ALIGN_TOP_MID, app.getRampTimeLabelLambda);
  lv_obj_set_grid_cell(rampLabel_, LV_GRID_ALIGN_CENTER, 1, 1,
                       LV_GRID_ALIGN_CENTER, 2, 1);

  //  ============================= FORWARD BUTTON =============================
  forwardButton_ = sharedDisplay.createActionButton(
      grid_cont, "FORWARD", app.increaseUserSpeedLambda,
      lv_color_hex(colorConfig.forwardColor));
  lv_obj_set_grid_cell(forwardButton_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);

  //  ============================= STOP BUTTON =============================
  stopButton_ = sharedDisplay.createActionButton(
      grid_cont, "STOP", app.setUserSpeedZeroLambda,
      lv_color_hex(colorConfig.stopColor));
  lv_obj_set_grid_cell(stopButton_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);

  //  ============================= REVERSE BUTTON =============================
  reverseButton_ = sharedDisplay.createActionButton(
      grid_cont, "REVERSE", app.decreaseUserSpeedLambda,
      lv_color_hex(colorConfig.reverseColor));
  lv_obj_set_grid_cell(reverseButton_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);

  //  ========================= RAMP DOWN BUTTON ===========================
  rampDownButton_ = sharedDisplay.createActionButton(
      grid_cont, "RAMP -", app.decreaseRampTimeLambda,
      lv_color_hex(colorConfig.inactiveColor));
  lv_obj_set_grid_cell(rampDownButton_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 4, 1);

  //  ============================= RAMP UP BUTTON =============================
  rampUpButton_ = sharedDisplay.createActionButton(
      grid_cont, "RAMP +", app.increaseRampTimeLambda,
      lv_color_hex(colorConfig.inactiveColor));
  lv_obj_set_grid_cell(rampUpButton_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 4, 1);

  //  ============================= LED 1 BUTTON =============================
  led1Button_ = sharedDisplay.createActionButton(
      grid_cont, "LED 1", app.toggleLed1StateLambda,
      lv_color_hex(colorConfig.inactiveColor), app.getLed1StateLambda);
  lv_obj_set_grid_cell(led1Button_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 4, 1);

  //  ============================= LED 3 BUTTON ============================
  led2Button_ = sharedDisplay.createActionButton(
      grid_cont, "LED 2", app.toggleLed2StateLambda,
      lv_color_hex(colorConfig.inactiveColor), app.getLed2StateLambda);

  lv_obj_set_grid_cell(led2Button_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 5, 1);

  //  ============================= AUTO BUTTON =============================
  autoButton_ = sharedDisplay.createActionButton(
      grid_cont, "AUTO", app.toggleAutoStateLambda,
      lv_color_hex(colorConfig.inactiveColor), app.getAutoStateLambda);
  lv_obj_set_grid_cell(autoButton_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 5, 1);

  //  ============================= AUDIO BUTTON =============================
  audioButton_ = sharedDisplay.createActionButton(
      grid_cont, "AUDIO", app.toggleAmbianceAudioStateLambda,
      lv_color_hex(colorConfig.inactiveColor), app.getAmbianceAudioStateLambda);
  lv_obj_set_grid_cell(audioButton_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 5, 1);

  //  ============================= INFO LABEL =============================
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

  refresh();
}
int lastSend = 0;
int sendPeriod = 100;
void LvDisplayController::sendStateToReceiver() {

  // Serial.println("called sendStateToReceiver portable");
  if (!hasStateChanged()) {
    return;
  }
  if (millis() - lastSend <= sendPeriod) {
    return;
  }
  lastSend = millis();
  JsonDocument state;

  getCondensedJsonFromState(state);
  if (state.isNull()) {
    return;
  }
  String outputString;
  serializeJson(state, outputString);
  host_->sendJson(outputString.c_str());
  // Serial.println("state sent");

  applyStateToUi();
}

void LvDisplayController::applyStateToUi() { sharedDisplay.updateStates(); }
