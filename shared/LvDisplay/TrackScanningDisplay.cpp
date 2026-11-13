#include "TrackScanningDisplay.h"

TrackScanningDisplay::TrackScanningDisplay(
    lv_obj_t* parent, std::function<void()> sendStateToReceiverCallback_) {
  sendStateToReceiverCallback = sendStateToReceiverCallback_;
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
      container_, "Curved\nStraight\nCross\nStation", 110, 128, 100, 40,
      {[this]() {
         app.setSelectedTrackType(CURVED);
         refresh();
       },
       [this]() {
         app.setSelectedTrackType(STRAIGHT);
         refresh();
       },
       [this]() {
         app.setSelectedTrackType(CROSS);
         refresh();
       },
       [this]() {
         app.setSelectedTrackType(STATION);
         refresh();
       }});

  float diameter = 36.0;
  float length = (diameter * M_PI) / 8;
  Uid emptyUid;

  Track templateTrack(CURVED, length, diameter / 2, 0, 0, 0, true, emptyUid,
                      emptyUid, emptyUid, emptyUid, true);

  app.setTemplateTrack(templateTrack);
  setListAsCurved();
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
  display_.setButtonState(btn_register_,
                          app.getScannerMode() == ScannerMode::REGISTER_TRACKS);
  display_.setButtonState(btn_write_,
                          app.getScannerMode() == ScannerMode::WRITING_TRACKS);
  display_.setButtonState(btn_scan_,
                          app.getScannerMode() == ScannerMode::SCANNING_TRACKS);
  display_.setButtonState(btn_auto_,
                          app.getScannerMode() == ScannerMode::PATH_FINDING);
  updateStatusLabel();
  if (app.getPreviousSelectedTrackType() != app.getSelectedTrackType()) {
    refreshList();
  }
}

void TrackScanningDisplay::refreshAndSend() {
  refresh();
  if (sendStateToReceiverCallback != nullptr) {
    sendStateToReceiverCallback();
  }
}

void TrackScanningDisplay::refreshList() {
  if (list_tracks_options_ != nullptr) {
    lv_obj_del_async(list_tracks_options_);
    if (app.getSelectedTrackType() == CURVED) {
      app.setSelectedTrackType(CURVED);

      setListAsCurved();

    } else if (app.getSelectedTrackType() == STRAIGHT) {
      app.setSelectedTrackType(STRAIGHT);

      setListAsStraight();
    } else if (app.getSelectedTrackType() == CROSS) {
      app.setSelectedTrackType(CROSS);
      setListAsNone();
    } else if (app.getSelectedTrackType() == STATION) {
      app.setSelectedTrackType(STATION);
      setListAsNone();
    }
  }
}


void TrackScanningDisplay::setListAsCurved() {
  Serial.println("set track as curved");

  app.getTemplateTrack()->setTrackType(CURVED);
  list_tracks_options_ = display_.createActionDropdown(
      container_, "O31\nO36\nO48\nO60\nO72\nO84\nO96\n", 215, 128, 100, 40,
      {[this]() {
         app.getTemplateTrack()->setSectionsAndRadius(8.0, 31.0 / 2.0);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setSectionsAndRadius(8.0, 36.0 / 2.0);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setSectionsAndRadius(12.0, 48.0 / 2.0);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setSectionsAndRadius(16.0, 60.0 / 2.0);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setSectionsAndRadius(16.0, 72.0 / 2.0);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setSectionsAndRadius(32.0, 84.0 / 2.0);
         refreshAndSend();
       },
       [this]() { app.getTemplateTrack()->setSectionsAndRadius(32.0, 96.0); }});
}
void TrackScanningDisplay::setListAsStraight() {
  Serial.println("set track as straight");
  app.getTemplateTrack()->setTrackType(STRAIGHT);

  list_tracks_options_ = display_.createActionDropdown(
      container_, "1 3/8\n1 3/4\n3\n4 1/2\n5\n10\n30\n", 215, 128, 100, 40,
      {[this]() {
         app.getTemplateTrack()->setLength(1.375);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setLength(1.75);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setLength(3.0);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setLength(4.5);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setLength(5.0);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setLength(10.0);
         refreshAndSend();
       },
       [this]() {
         app.getTemplateTrack()->setLength(30.0);
         refreshAndSend();
       }});
}
void TrackScanningDisplay::setListAsNone() {
  list_tracks_options_ = display_.createActionDropdown(
      container_, "no options", 215, 128, 100, 40, {[this]() {}});
}
// Scan button
void TrackScanningDisplay::registerTracks() {
  app.setScannerMode(REGISTER_TRACKS);
  refreshAndSend();
}

void TrackScanningDisplay::writeTracks() {
  app.setScannerMode(WRITING_TRACKS);
  refreshAndSend();
}

void TrackScanningDisplay::scanNewLayout() {
  app.setScannerMode(SCANNING_TRACKS);
  refreshAndSend();
}

void TrackScanningDisplay::pathFind() {
  app.setScannerMode(PATH_FINDING);
  refreshAndSend();
}

void TrackScanningDisplay::toggle() {
  if (lv_obj_is_hidden(container_)) {
    lv_obj_move_foreground(container_);
    lv_obj_set_hidden(container_, false);

  } else {
    lv_obj_set_hidden(container_, true);
  }
}