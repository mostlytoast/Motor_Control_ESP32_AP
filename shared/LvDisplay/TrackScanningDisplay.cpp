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
}

TrackScanningDisplay::~TrackScanningDisplay() { lv_obj_delete(container_); }

// --- Logic Methods ---

void TrackScanningDisplay::registerTracks() {
  app.setScannerMode(REGISTER_TRACKS);
}

void TrackScanningDisplay::writeTracks() { app.setScannerMode(WRITING_TRACKS); }

void TrackScanningDisplay::scanNewLayout() {
  app.setScannerMode(SCANNING_TRACKS);
}

void TrackScanningDisplay::pathFind() { app.setScannerMode(PATH_FINDING); }

void TrackScanningDisplay::toggle() {
  if (lv_obj_is_hidden(container_)) {
    lv_obj_move_foreground(container_);
    lv_obj_set_hidden(container_, false);

  } else {
    lv_obj_set_hidden(container_, true);
  }
}