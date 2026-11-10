#ifndef RACK_SCANNING_DISPLAY_H

#define RACK_SCANNING_DISPLAY_H

#include <lvgl.h>

#include <functional>
#include <iostream>
#include <vector>

#include "../utils/AppState.h"  // Assuming app.h contains setScannerMode
#include "LvTrackMap.h"
#include "SharedDisplayCalls.h"

class TrackScanningDisplay {
 public:
  using SendStateToReceiverCallback = std::function<void()>;

  TrackScanningDisplay(lv_obj_t* parent,
                       std::function<void()> sendStateToReceiverCallback_);

  ~TrackScanningDisplay();

  void refresh();

  // Methods to toggle visibility
  void toggle();

 private:
  lv_obj_t* container_;
  lv_obj_t* scan_status_label_;
  lv_obj_t* btn_register_;
  lv_obj_t* btn_write_;
  lv_obj_t* btn_scan_;
  lv_obj_t* btn_auto_;
  lv_obj_t* list_tracks_;
  lv_obj_t* list_tracks_options_ = nullptr;
  lv_obj_t* hide_button_;

  lv_coord_t lastWidth_ = 0;
  lv_coord_t lastHeight_ = 0;

  // Helper to handle selection styling
  void setListAsCurved();
  void setListAsStraight();
  void setListAsNone();
  void registerTracks();
  void writeTracks();
  void scanNewLayout();
  void pathFind();
  void refreshList();
  void refreshAndSend();
  void updateStatusLabel();

  SharedDisplayCalls display_;

  SendStateToReceiverCallback sendStateToReceiverCallback = nullptr;
  Track templateTrack;
};

#endif  // LVSHARED_LVDISPLAY_TRACK_SCANNING_DISPLAY_H
