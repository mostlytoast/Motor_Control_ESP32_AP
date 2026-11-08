#include "TrackScanningDisplay.h"

TrackScanningDisplay::TrackScanningDisplay(lv_obj_t* parent) {
  // Create a container for the buttons to allow easy hiding/showing
  // Using SharedDisplayCalls to create a container if available,
  // otherwise standard lv_obj_create
  container_ = lv_obj_create(parent);
  if (!container_) {
    Serial.println("FAILED creating container");
    return;
  }

  lv_obj_set_size(container_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(container_, 0, 0);

  lv_obj_set_style_border_side(container_, LV_BORDER_SIDE_NONE, 0);
  lv_obj_set_scrolled(container_, LV_SCROLL_SNAP_NONE);

  scan_status_label_ = display_.createLabel(container_, kColorStatus, -8, 53,
                                            LV_PART_MAIN, LV_ALIGN_TOP_RIGHT);

  btn_register_ = display_.createActionButton(
      container_, "Register", 5, 78, 100, 45, [this]() { registerTracks(); },
      kColorForward);

  btn_write_ = display_.createActionButton(
      container_, "Write", 110, 78, 100, 45, [this]() { writeTracks(); },
      kColorForward);

  btn_scan_ = display_.createActionButton(
      container_, "Scan", 215, 78, 100, 45, [this]() { scanNewLayout(); },
      kColorForward);

  btn_auto_ = display_.createActionButton(
      container_, "Auto", 5, 128, 100, 40, [this]() { pathFind(); },
      kColorForward);

  hide_button_ = display_.createActionButton(
      container_, "Hide", 5, 27, 100, 22, [this]() { toggle(); },
      kColorForward);

  list_tracks_ = display_.createActionDropdown(
      container_, "Curved\nStraight\nCross", 110, 128, 100, 40,
      {[this]() { Serial.println("Curved"); },
       [this]() { Serial.println("Straight"); },
       [this]() { Serial.println("Cross"); }});

  lv_obj_set_hidden(container_, true);
  refresh();
}

TrackScanningDisplay::~TrackScanningDisplay() { lv_obj_delete(container_); }

// --- Logic Methods ---

void TrackScanningDisplay::updateStatusLabel() {
  if (scan_status_label_ == nullptr) {
    return;
  }
  if (!app.getStatusLabelText().equals("")) {
    // Todo use own vars for this? include trackscanner
    display_.setLabelText(scan_status_label_,
                          app.getScannerStatusLabelText().c_str());
    return;
  }
}
void TrackScanningDisplay::refresh() {
  Serial.println("TrackScanningDisplay refresh");

  display_.setButtonState(btn_register_,
                          app.getScannerMode() == ScannerMode::REGISTER_TRACKS);
  display_.setButtonState(btn_write_,
                          app.getScannerMode() == ScannerMode::WRITING_TRACKS);
  display_.setButtonState(btn_scan_,
                          app.getScannerMode() == ScannerMode::SCANNING_TRACKS);
  display_.setButtonState(btn_auto_,
                          app.getScannerMode() == ScannerMode::PATH_FINDING);
  updateStatusLabel();
}
void TrackScanningDisplay::registerTracks() {
  app.setScannerMode(REGISTER_TRACKS);
  Serial.println("set scan mode to REGISTER_TRACKS");
  refresh();
  sendStateToReceiver()
}

void TrackScanningDisplay::writeTracks() {
  app.setScannerMode(WRITING_TRACKS);
  Serial.println("set scan mode to WRITING_TRACKS ");
  refresh();
  sendStateToReceiver()
}

void TrackScanningDisplay::scanNewLayout() {
  app.setScannerMode(SCANNING_TRACKS);
  Serial.println("set scan mode to SCANNING_TRACKS");
  refresh();
  sendStateToReceiver()
}

void TrackScanningDisplay::pathFind() {
  app.setScannerMode(PATH_FINDING);
  Serial.println("set scan mode to PATH_FINDING");
  refresh();
  sendStateToReceiver()


}

void TrackScanningDisplay::toggle() {
  if (lv_obj_is_hidden(container_)) {
    lv_obj_move_foreground(container_);
    lv_obj_set_hidden(container_, false);

  } else {
    lv_obj_set_hidden(container_, true);
  }
}

void TrackScanningDisplay::sendStateToReceiver() {
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

  applyStateToUi();
}