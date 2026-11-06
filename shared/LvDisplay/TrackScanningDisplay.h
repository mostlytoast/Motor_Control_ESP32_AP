#ifndef RACK_SCANNING_DISPLAY_H

#define RACK_SCANNING_DISPLAY_H

#include <lvgl.h>

#include <iostream>
#include <vector>


#include "../utils/AppState.h"  // Assuming app.h contains setScannerMode
#include "LvTrackMap.h"
#include "SharedDisplayCalls.h"

class TrackScanningDisplay : public LvTrackMap {
 public:
  explicit TrackScanningDisplay(lv_obj_t* parent = nullptr);
  ~TrackScanningDisplay();

  void registerTracks();
  void writeTracks();
  void scanNewLayout();
  void pathFind();

  // Methods to toggle visibility
  void toggle();

 private:
  lv_obj_t* container_;
  lv_obj_t* btn_register_;
  lv_obj_t* btn_write_;
  lv_obj_t* btn_scan_;
  lv_obj_t* btn_auto_;
  lv_obj_t* list_tracks_;
  lv_obj_t* hide_button_;

  lv_coord_t lastWidth_ = 0;
  lv_coord_t lastHeight_ = 0;

  // Static callback to handle button presses
  //   static void button_event_callback(lv_obj_t* obj,
  //                                     lv_event_contents_t* contents);

  // Helper to handle selection styling
  void set_selected_button(lv_obj_t* selected_btn);
  SharedDisplayCalls display_;
};

#endif  // LVSHARED_LVDISPLAY_TRACK_SCANNING_DISPLAY_H
