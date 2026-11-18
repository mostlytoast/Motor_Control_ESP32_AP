#include "TrackScanningDisplay.h"

TrackScanningDisplay::TrackScanningDisplay(
    lv_obj_t* parent, std::function<void()> sendStateToReceiverCallback_) {
  sendStateToReceiverCallback = sendStateToReceiverCallback_;
  // Create a container for the buttons to allow easy hiding/showing
  // Using SharedDisplayCalls to create a container if available,
  // otherwise standard lv_obj_create

  root_ = lv_obj_create(parent);
  if (!root_) {
    Serial.println("FAILED creating container");
    return;
  }
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_align(root_, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_center(root_);
  lv_obj_set_style_bg_color(root_, lv_color_hex(colorConfig.backgroundColor),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);

  // lv_obj_set_style_border_side(root_, LV_BORDER_SIDE_NONE, 0);
  lv_obj_set_scrolled(root_, LV_SCROLL_SNAP_NONE);

  lv_obj_set_scrollable(root_, false);

  grid_cont = lv_obj_create(root_);
  lv_obj_set_width(grid_cont, lv_pct(100));
  lv_obj_set_height(grid_cont, lv_pct(100));

  /* Center the container on the screen */
  lv_obj_center(grid_cont);
  lv_obj_set_style_bg_color(
      grid_cont, lv_color_hex(colorConfig.backgroundColor), LV_PART_MAIN);
  static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_TEMPLATE_LAST};

  /* Define 2 rows: equal height */
  static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(3),
                              LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(grid_cont, col_dsc, row_dsc);
  lv_obj_set_layout(grid_cont, LV_LAYOUT_GRID);
  hide_button_ = sharedDisplay.createActionButton(
      grid_cont, "Hide", [this](lv_event_t* event) { toggle(); },
      lv_color_hex(colorConfig.menuHeaderColor));
  lv_obj_set_grid_cell(hide_button_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 0, 1);

  scan_status_label_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.statusColor), 8, 53, LV_PART_MAIN,
      LV_ALIGN_TOP_LEFT, "");
  lv_obj_set_grid_cell(scan_status_label_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);
  btn_register_ = sharedDisplay.createActionButton(
      grid_cont, "Register", [this](lv_event_t* event) { registerTracks(); },
      lv_color_hex(colorConfig.inactiveColor), [this]() { return app.getScannerMode() == REGISTER_TRACKS ; });
  lv_obj_set_grid_cell(btn_register_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 2, 1);
  btn_write_ = sharedDisplay.createActionButton(
      grid_cont, "Write", [this](lv_event_t* event) { writeTracks(); },
      lv_color_hex(colorConfig.inactiveColor),[this]() { return app.getScannerMode() == WRITING_TRACKS ; });
  lv_obj_set_grid_cell(btn_write_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 2, 1);
  btn_scan_ = sharedDisplay.createActionButton(
      grid_cont, "Scan", [this](lv_event_t* event) { scanNewLayout(); },
      lv_color_hex(colorConfig.inactiveColor),[this]() { return app.getScannerMode() == SCANNING_TRACKS ; });
  lv_obj_set_grid_cell(btn_scan_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 2, 1);
  btn_normal_ = sharedDisplay.createActionButton(
      grid_cont, "Normal", [this](lv_event_t* event) { normal(); },
      lv_color_hex(colorConfig.inactiveColor),[this]() { return app.getScannerMode() == NORMAL ; });
  lv_obj_set_grid_cell(btn_normal_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);

  list_tracks_ = sharedDisplay.createActionDropdown(
      grid_cont, "Curved\nStraight\nCross\nStation", 110, 128, 100, 40,
      {[this](lv_event_t* event) {
         app.setSelectedTrackType(CURVED);
         refresh();
       },
       [this](lv_event_t* event) {
         app.setSelectedTrackType(STRAIGHT);

         refresh();
       },
       [this](lv_event_t* event) {
         app.setSelectedTrackType(CROSS);
         refresh();
       },
       [this](lv_event_t* event) {
         app.setSelectedTrackType(STATION);
         refresh();
       }});
  lv_obj_set_grid_cell(list_tracks_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);

  float diameter = 36.0;
  float length = (diameter * M_PI) / 8;
  Uid emptyUid;
  Track* templateTrack =
      trackMap.createTrack(CURVED, length, diameter / 2, 0, 0, 0, true,
                           emptyUid, emptyUid, emptyUid, emptyUid, true);
  if (templateTrack == nullptr) {
    return;
  }

  app.setTemplateTrack(*templateTrack);

  if (app.getTemplateTrack() == nullptr) {
    return;
  }

  setListAsCurved();

  lv_obj_set_hidden(root_, true);
  refresh();
  refreshList();
}

TrackScanningDisplay::~TrackScanningDisplay() { lv_obj_delete(root_); }

// --- Logic Methods ---

void TrackScanningDisplay::updateStatusLabel() {
  if (scan_status_label_ == nullptr) {
    return;
  }
  // if (!app.getStatusLabelText().equals("")) {
  // Serial.print("set track status label");
  // Serial.println(app.getScannerStatusLabelText());
  // Todo use own vars for this? include trackscanner
  sharedDisplay.setLabelText(scan_status_label_,
                             app.getScannerStatusLabelText().c_str());

  return;
  // }
}
void TrackScanningDisplay::refresh() {
  if (lv_obj_is_hidden(root_)) {
    return;
  }
  sharedDisplay.updateStates();
  refreshList();
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
    list_tracks_options_ = nullptr;
    // lv_obj_set_grid_cell(list_tracks_options_, LV_GRID_ALIGN_STRETCH, 2, 1,
    //                      LV_GRID_ALIGN_STRETCH, 3, 1);
    if (app.getSelectedTrackType() == CURVED &&
        app.getPreviousSelectedTrackType() != CURVED) {
      app.setSelectedTrackType(CURVED);

      setListAsCurved();

    } else if (app.getSelectedTrackType() == STRAIGHT &&
               app.getPreviousSelectedTrackType() != STRAIGHT) {
      app.setSelectedTrackType(STRAIGHT);

      setListAsStraight();
    } else if (app.getSelectedTrackType() == CROSS &&
               app.getPreviousSelectedTrackType() != CROSS) {
      app.setSelectedTrackType(CROSS);
      setListAsNone();
    } else if (app.getSelectedTrackType() == STATION &&
               app.getPreviousSelectedTrackType() != STATION) {
      app.setSelectedTrackType(STATION);
      setListAsNone();
    }
  }
}

void TrackScanningDisplay::setListAsCurved() {
  Serial.println("set track as curved");

  Track* track = app.getTemplateTrack();

  if (track == nullptr) {
    return;
  }

  track->setTrackType(CURVED);

  track->setSectionsAndRadius(8.0, 31.0 / 2.0);
  track->setDirection(false);
  list_tracks_options_ = sharedDisplay.createActionDropdown(
      grid_cont, "O31\nO36\nO48\nO60\nO72\nO84\nO96\n", 215, 128, 100, 40,
      {[this](lv_event_t* event) {
         app.getTemplateTrack()->setSectionsAndRadius(8.0, 31.0 / 2.0);

         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setSectionsAndRadius(8.0, 36.0 / 2.0);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setSectionsAndRadius(12.0, 48.0 / 2.0);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setSectionsAndRadius(16.0, 60.0 / 2.0);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setSectionsAndRadius(16.0, 72.0 / 2.0);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setSectionsAndRadius(32.0, 84.0 / 2.0);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setSectionsAndRadius(32.0, 96.0 / 2);
         refreshAndSend();
       }});
  lv_obj_set_grid_cell(list_tracks_options_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);
}
void TrackScanningDisplay::setListAsStraight() {
  Track* track = app.getTemplateTrack();

  if (track == nullptr) {
    return;
  }

  track->setTrackType(STRAIGHT);
  track->setRadius(0);
  track->setLength(1.375);
  track->setDirection(false);

  list_tracks_options_ = sharedDisplay.createActionDropdown(
      grid_cont, "1 3/8\n1 3/4\n3\n4 1/2\n5\n10\n30\n", 215, 128, 100, 40,
      {[this](lv_event_t* event) {
         app.getTemplateTrack()->setLength(1.375);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setLength(1.75);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setLength(3.0);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setLength(4.5);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setLength(5.0);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setLength(10.0);
         refreshAndSend();
       },
       [this](lv_event_t* event) {
         app.getTemplateTrack()->setLength(30.0);
         refreshAndSend();
       }});
  lv_obj_set_grid_cell(list_tracks_options_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);
}
void TrackScanningDisplay::setListAsNone() {
  list_tracks_options_ =
      sharedDisplay.createActionDropdown(grid_cont, "no options", 215, 128, 100,
                                         40, {[this](lv_event_t* event) {}});
  lv_obj_set_grid_cell(list_tracks_options_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);
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

void TrackScanningDisplay::normal() {
  app.setScannerMode(NORMAL);
  refreshAndSend();
}

void TrackScanningDisplay::toggle() {
  if (lv_obj_is_hidden(root_)) {
    // lv_obj_move_to_index(root_, 0);

    lv_obj_move_foreground(root_);
    lv_obj_set_hidden(root_, false);

  } else {
    // lv_obj_move_to_index(root_, 2);

    lv_obj_set_hidden(root_, true);
  }
}