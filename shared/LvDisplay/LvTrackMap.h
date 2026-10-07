#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "../track/map.h"

/*
 * LVGL renderer for the application's TrackMap.
 *
 * TrackMap owns the track data and is responsible for:
 *   - creating tracks
 *   - connecting tags
 *   - calculating all track poses
 *
 * LvTrackMap only renders that data.
 */
class LvTrackMap {
 public:
  struct Config {
    int32_t width = 320;
    int32_t height = 200;
    int32_t padding = 18;

    lv_color_t background = lv_color_hex(0xD9D9D9);
    lv_color_t grid = lv_color_hex(0xE4E4E4);
    lv_color_t track = lv_color_hex(0x444444);
    lv_color_t trackCenter = lv_color_hex(0xC9C9C9);
    lv_color_t tagConnected = lv_color_hex(0x00A83B);
    lv_color_t tagUnconnected = lv_color_hex(0xB00020);
    lv_color_t connection = lv_color_hex(0x777777);
    lv_color_t label = lv_color_hex(0x003249);
    lv_color_t bumper = lv_color_hex(0xB00020);

    uint8_t trackWidth = 10;
    uint8_t centerWidth = 3;
    uint8_t connectionWidth = 2;
    uint8_t tagRadius = 5;
  };

  explicit LvTrackMap(lv_obj_t* parent = nullptr);
  ~LvTrackMap();

  // void createTrackMap(lv_obj_t* parent);

  // static void mapMenuEvent(lv_event_t* event);

  lv_obj_t* object() const { return root_; }

  void setConfig(const Config& config);

  const Config& config() const { return config_; }

  // Attach the LVGL renderer to the application's TrackMap.
  //
  // TrackMap must remain alive while LvTrackMap is being used.
  void setTrackMap(TrackMap* map);

  void setTrackMap(TrackMap& map) { setTrackMap(&map); }

  TrackMap* trackMap() const { return map_; }

  void clear();
  void refresh();

  size_t trackCount() const;

  // Recalculate the display transform only.
  // This does NOT recalculate track geometry.
  void resetView();

  static void refreshButtonEvent(lv_event_t* e);
  static void resetButtonEvent(lv_event_t* e);

 private:
  using Point = ::Point;
  
  struct Transform {
    float minX = 0;
    float minY = 0;
    float scale = 1;
  };

  lv_obj_t* root_ = nullptr;

  Config config_;

  // The actual application map.
  TrackMap* map_ = nullptr;

  Transform transform_;

  void installCallbacks();

  static void drawEvent(lv_event_t* e);

  void calculateTransform();

  Point worldToMap(Point p) const;

  void drawBackground(lv_layer_t* layer, const lv_area_t& area) const;

  void drawConnections(lv_layer_t* layer) const;

  void drawTrack(lv_layer_t* layer, const Track& track, size_t index) const;

  void drawStraight(lv_layer_t* layer, Point a, Point b) const;

  void drawCurve(lv_layer_t* layer, const Track& track) const;

  void drawSwitch(lv_layer_t* layer, const Track& track) const;

  void drawCross(lv_layer_t* layer, const Track& track) const;

  void drawBumper(lv_layer_t* layer, const Track& track) const;

  void drawTags(lv_layer_t* layer, const Track& track) const;

  void drawLabel(lv_layer_t* layer, const Track& track, size_t index) const;

  Point tagWorldPosition(const Track& track, const Tag& tag) const;

  int trackIndex(const Track* track) const;

  void drawSegment(lv_layer_t* layer, Point a, Point b, lv_color_t color,
                   uint8_t width) const;

  void drawCircle(lv_layer_t* layer, Point center, int32_t radius,
                  lv_color_t color, bool filled) const;

  void drawText(lv_layer_t* layer, Point center, const char* text,
                lv_color_t color, bool small = false) const;
};