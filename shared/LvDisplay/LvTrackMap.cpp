#include "LvTrackMap.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
// LvTrackMap trackMapDisplay(screen);
#include "../track/map.h"

namespace {

// void LvTrackMap::createTrackMap(lv_obj_t* parent) {
//   trackMapDisplay = new LvTrackMap(parent);

//   trackMapDisplay->setTrackMap(trackMap);
//   lv_obj_add_flag(trackMapDisplay, LV_OBJ_FLAG_HIDDEN);

// }

// static void LvTrackMap::mapMenuEvent(lv_event_t* event) {
//   if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
//     return;
//   }

//   if (trackMapDisplay == NULL) {
//     return;
//   }

//   if (lv_obj_has_flag(trackMapDisplay, LV_OBJ_FLAG_HIDDEN)) {
//     lv_obj_clear_flag(trackMapDisplay, LV_OBJ_FLAG_HIDDEN);

//     receiverListNeedsRefresh = true;
//   }

//   else {
//     closeControllerMenu();
//   }
// }
constexpr float PI_F = 3.14159265358979323846f;
constexpr float DEG_TO_RAD_F = PI_F / 180.0f;

static const char* trackTypeName(TrackType type) {
  switch (type) {
    case STRAIGHT:
      return "STRAIGHT";
    case CURVED:
      return "CURVED";
    case SWITCH:
      return "SWITCH";
    case BUMPER:
      return "BUMPER";
    case CROSS:
      return "CROSS";
    default:
      return "UNKNOWN";
  }
}

static float normalizeAngle(float angle) {
  while (angle > PI_F) {
    angle -= 2.0f * PI_F;
  }

  while (angle < -PI_F) {
    angle += 2.0f * PI_F;
  }

  return angle;
}

}  // namespace

// ============================================================
// CONSTRUCTION / CONFIGURATION
// ============================================================

LvTrackMap::LvTrackMap(lv_obj_t* parent) {
  root_ = lv_obj_create(parent);

  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  //   lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(root_, LV_DIR_NONE);

  installCallbacks();
}

LvTrackMap::~LvTrackMap() {
  // root_ is owned by LVGL/the parent object.
  // Do not delete it here.
  root_ = nullptr;
}

void LvTrackMap::installCallbacks() {
  if (!root_) {
    return;
  }

  lv_obj_add_event_cb(root_, drawEvent, LV_EVENT_DRAW_MAIN, this);
}

void LvTrackMap::setConfig(const Config& config) {
  config_ = config;
  calculateTransform();
  refresh();
}

void LvTrackMap::setTrackMap(TrackMap* map) {
  map_ = map;

  calculateTransform();
  refresh();
}

void LvTrackMap::clear() {
  // LvTrackMap does not own the TrackMap.
  //
  // This only clears the display's reference. The actual map must be
  // cleared by its owner.
  map_ = nullptr;

  transform_ = {};

  refresh();
}

size_t LvTrackMap::trackCount() const {
  if (!map_) {
    return 0;
  }

  return map_->getTrackCount();
}

// ============================================================
// VIEW / TRANSFORM
// ============================================================

void LvTrackMap::calculateTransform() {
  transform_ = {};
  transform_.scale = 1.0f;

  if (!map_) {
    return;
  }

  const uint8_t count = map_->getTrackCount();

  if (count == 0) {
    return;
  }

  float minX = INFINITY;
  float minY = INFINITY;
  float maxX = -INFINITY;
  float maxY = -INFINITY;

  bool foundPoint = false;

  for (uint8_t i = 0; i < count; ++i) {
    const Track* track = map_->getTrack(i);

    if (!track || !track->positioned) {
      continue;
    }

    const TrackPose& pose = track->pose;

    const Point points[] = {pose.entrance, pose.exit};

    for (const Point& point : points) {
      if (!isfinite(point.x) || !isfinite(point.y)) {
        continue;
      }

      minX = fminf(minX, point.x);
      minY = fminf(minY, point.y);
      maxX = fmaxf(maxX, point.x);
      maxY = fmaxf(maxY, point.y);

      foundPoint = true;
    }

    // Only SWITCH currently produces a meaningful exitB in map.cpp.
    // Do not include unused zero-initialized B points from other
    // track types because that would distort the view transform.
    if (track->trackType == SWITCH) {
      if (isfinite(pose.exitB.x) && isfinite(pose.exitB.y)) {
        minX = fminf(minX, pose.exitB.x);
        minY = fminf(minY, pose.exitB.y);
        maxX = fmaxf(maxX, pose.exitB.x);
        maxY = fmaxf(maxY, pose.exitB.y);

        foundPoint = true;
      }
    }
  }

  if (!foundPoint || !isfinite(minX) || !isfinite(minY) || !isfinite(maxX) ||
      !isfinite(maxY)) {
    return;
  }

  const float worldWidth = fmaxf(maxX - minX, 1.0f);
  const float worldHeight = fmaxf(maxY - minY, 1.0f);

  lv_coord_t width = lv_obj_get_width(root_);
  lv_coord_t height = lv_obj_get_height(root_);

  if (width <= 0) {
    width = config_.width;
  }

  if (height <= 0) {
    height = config_.height;
  }

  const float padding = static_cast<float>(config_.padding);

  const float availableWidth =
      fmaxf(static_cast<float>(width) - padding * 2.0f, 1.0f);

  const float availableHeight =
      fmaxf(static_cast<float>(height) - padding * 2.0f, 1.0f);

  transform_.minX = minX;
  transform_.minY = minY;

  transform_.scale =
      fminf(availableWidth / worldWidth, availableHeight / worldHeight);

  // Prevent an invalid transform if the LVGL object is not fully sized yet.
  if (!isfinite(transform_.scale) || transform_.scale <= 0.0f) {
    transform_.scale = 1.0f;
  }
}

void LvTrackMap::resetView() {
  calculateTransform();
  refresh();
}

LvTrackMap::Point LvTrackMap::worldToMap(Point point) const {
  Point result;

  result.x = (point.x - transform_.minX) * transform_.scale +
             static_cast<float>(config_.padding);

  result.y = (point.y - transform_.minY) * transform_.scale +
             static_cast<float>(config_.padding);

  return result;
}

// ============================================================
// REFRESH
// ============================================================

void LvTrackMap::refresh() {
  if (root_) {
    lv_obj_invalidate(root_);
  }
}

// ============================================================
// DRAW EVENT
// ============================================================

void LvTrackMap::drawEvent(lv_event_t* e) {
  if (!e) {
    return;
  }

  LvTrackMap* self = static_cast<LvTrackMap*>(lv_event_get_user_data(e));

  if (!self || !self->root_) {
    return;
  }

  lv_layer_t* layer = lv_event_get_layer(e);

  if (!layer) {
    return;
  }

  lv_area_t area;
  lv_obj_get_coords(self->root_, &area);

  self->drawBackground(layer, area);
  self->drawConnections(layer);

  if (self->map_) {
    const uint8_t count = self->map_->getTrackCount();

    for (uint8_t i = 0; i < count; ++i) {
      const Track* track = self->map_->getTrack(i);

      if (!track) {
        continue;
      }

      self->drawTrack(layer, *track, i);
    }
  }

  char countText[24];

  snprintf(countText, sizeof(countText), "Tracks: %u",
           static_cast<unsigned>(self->trackCount()));

  self->drawText(layer,
                 {static_cast<float>(lv_area_get_width(&area)) / 2.0f, 10.0f},
                 countText, self->config_.label);
}

// ============================================================
// BACKGROUND
// ============================================================

void LvTrackMap::drawBackground(lv_layer_t* layer,
                                const lv_area_t& area) const {
  lv_draw_rect_dsc_t bg;
  lv_draw_rect_dsc_init(&bg);

  bg.bg_color = config_.background;
  bg.bg_opa = LV_OPA_COVER;
  bg.border_width = 0;

  lv_draw_rect(layer, &bg, &area);

  constexpr int32_t spacing = 20;

  const int32_t width = lv_area_get_width(&area);
  const int32_t height = lv_area_get_height(&area);

  for (int32_t x = 0; x < width; x += spacing) {
    drawSegment(layer, {static_cast<float>(x), 0.0f},
                {static_cast<float>(x), static_cast<float>(height)},
                config_.grid, 1);
  }

  for (int32_t y = 0; y < height; y += spacing) {
    drawSegment(layer, {0.0f, static_cast<float>(y)},
                {static_cast<float>(width), static_cast<float>(y)},
                config_.grid, 1);
  }
}

// ============================================================
// LOW-LEVEL DRAW HELPERS
// ============================================================

void LvTrackMap::drawSegment(lv_layer_t* layer, Point a, Point b,
                             lv_color_t color, uint8_t width) const {
  if (!root_) {
    return;
  }

  lv_draw_line_dsc_t dsc;
  lv_draw_line_dsc_init(&dsc);

  dsc.color = color;
  dsc.width = width;
  dsc.round_start = true;
  dsc.round_end = true;

  lv_area_t rootArea;
  lv_obj_get_coords(root_, &rootArea);

  lv_point_precise_t points[2];

  points[0].x = rootArea.x1 + static_cast<lv_coord_t>(lroundf(a.x));

  points[0].y = rootArea.y1 + static_cast<lv_coord_t>(lroundf(a.y));

  points[1].x = rootArea.x1 + static_cast<lv_coord_t>(lroundf(b.x));

  points[1].y = rootArea.y1 + static_cast<lv_coord_t>(lroundf(b.y));

  dsc.points = points;

  lv_draw_line(layer, &dsc);
}

void LvTrackMap::drawCircle(lv_layer_t* layer, Point center, int32_t radius,
                            lv_color_t color, bool filled) const {
  if (!root_) {
    return;
  }

  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);

  dsc.radius = LV_RADIUS_CIRCLE;

  if (filled) {
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
  } else {
    dsc.bg_opa = LV_OPA_TRANSP;
    dsc.border_width = 2;
    dsc.border_color = color;
    dsc.border_opa = LV_OPA_COVER;
  }

  lv_area_t rootArea;
  lv_obj_get_coords(root_, &rootArea);

  lv_area_t area;

  area.x1 = rootArea.x1 + static_cast<lv_coord_t>(lroundf(center.x - radius));

  area.y1 = rootArea.y1 + static_cast<lv_coord_t>(lroundf(center.y - radius));

  area.x2 = rootArea.x1 + static_cast<lv_coord_t>(lroundf(center.x + radius));

  area.y2 = rootArea.y1 + static_cast<lv_coord_t>(lroundf(center.y + radius));

  lv_draw_rect(layer, &dsc, &area);
}

void LvTrackMap::drawText(lv_layer_t* layer, Point center, const char* text,
                          lv_color_t color, bool small) const {
  (void)small;

  if (!root_ || !text) {
    return;
  }

  lv_draw_label_dsc_t dsc;
  lv_draw_label_dsc_init(&dsc);

  dsc.color = color;
  dsc.font = LV_FONT_DEFAULT;
  dsc.align = LV_TEXT_ALIGN_CENTER;

  lv_area_t rootArea;
  lv_obj_get_coords(root_, &rootArea);

  lv_area_t area;

  area.x1 = rootArea.x1 + static_cast<lv_coord_t>(lroundf(center.x - 100.0f));

  area.x2 = rootArea.x1 + static_cast<lv_coord_t>(lroundf(center.x + 100.0f));

  area.y1 = rootArea.y1 + static_cast<lv_coord_t>(lroundf(center.y - 8.0f));

  area.y2 = rootArea.y1 + static_cast<lv_coord_t>(lroundf(center.y + 8.0f));

  dsc.text = text;

  lv_draw_label(layer, &dsc, &area);
}

// ============================================================
// CONNECTIONS
// ============================================================

void LvTrackMap::drawConnections(lv_layer_t* layer) const {
  if (!map_) {
    return;
  }

  const uint8_t count = map_->getTrackCount();

  for (uint8_t i = 0; i < count; ++i) {
    const Track* track = map_->getTrack(i);

    if (!track || !track->positioned) {
      continue;
    }

    const Tag* tags[] = {&track->entranceA, &track->entranceB, &track->exitA,
                         &track->exitB};

    for (const Tag* tag : tags) {
      if (!tag || !tag->connectedTag) {
        continue;
      }

      const Tag* otherTag = tag->connectedTag;

      if (!otherTag->track) {
        continue;
      }

      const Track* otherTrack = otherTag->track;

      if (!otherTrack->positioned) {
        continue;
      }

      const int otherIndex = trackIndex(otherTrack);

      if (otherIndex < 0) {
        continue;
      }

      // A connection is stored in both directions. Draw it only once.
      if (i > static_cast<uint8_t>(otherIndex)) {
        continue;
      }

      const Point aWorld = tagWorldPosition(*track, *tag);

      const Point bWorld = tagWorldPosition(*otherTrack, *otherTag);

      const Point a = worldToMap(aWorld);

      const Point b = worldToMap(bWorld);

      drawSegment(layer, a, b, config_.connection, config_.connectionWidth);
    }
  }
}

// ============================================================
// TRACK INDEX / TAG POSITION
// ============================================================

int LvTrackMap::trackIndex(const Track* track) const {
  if (!map_ || !track) {
    return -1;
  }

  const uint8_t count = map_->getTrackCount();

  for (uint8_t i = 0; i < count; ++i) {
    const Track* current = map_->getTrack(i);

    if (current == track) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

LvTrackMap::Point LvTrackMap::tagWorldPosition(const Track& track,
                                               const Tag& tag) const {
  if (&tag == &track.entranceA) {
    return track.pose.entrance;
  }

  if (&tag == &track.entranceB) {
    return track.pose.entranceB;
  }

  if (&tag == &track.exitA) {
    return track.pose.exit;
  }

  if (&tag == &track.exitB) {
    return track.pose.exitB;
  }

  return {};
}

// ============================================================
// TRACK DRAWING
// ============================================================

void LvTrackMap::drawTrack(lv_layer_t* layer, const Track& track,
                           size_t index) const {
  if (!track.positioned) {
    return;
  }

  switch (track.trackType) {
    case STRAIGHT:
      drawStraight(layer, worldToMap(track.pose.entrance),
                   worldToMap(track.pose.exit));
      break;

    case CURVED:
      drawCurve(layer, track);
      break;

    case SWITCH:
      drawSwitch(layer, track);
      break;

    case CROSS:
      drawCross(layer, track);
      break;

    case BUMPER:
      drawStraight(layer, worldToMap(track.pose.entrance),
                   worldToMap(track.pose.exit));

      drawBumper(layer, track);
      break;
  }

  drawTags(layer, track);
  drawLabel(layer, track, index);
}

// ============================================================
// STRAIGHT
// ============================================================

void LvTrackMap::drawStraight(lv_layer_t* layer, Point a, Point b) const {
  drawSegment(layer, a, b, config_.track, config_.trackWidth);

  drawSegment(layer, a, b, config_.trackCenter, config_.centerWidth);
}

// ============================================================
// CURVE
//
// IMPORTANT:
// This function only renders the curve.
//
// It does NOT calculate or modify track.pose.
//
// map.cpp is the source of truth for the curve endpoints and headings.
// ============================================================

void LvTrackMap::drawCurve(lv_layer_t* layer, const Track& track) const {
  const TrackPose& pose = track.pose;

  if (track.radius <= 0.0f) {
    drawStraight(layer, worldToMap(pose.entrance), worldToMap(pose.exit));

    return;
  }

  const float radius = track.radius;

  const float heading = pose.entranceAHeading * DEG_TO_RAD_F;

  // map.cpp uses:
  //
  //   true  -> heading - 90 degrees
  //   false -> heading + 90 degrees
  //
  // to locate the curve center.
  const float centerHeading = track.direction ? pose.entranceAHeading - 90.0f
                                              : pose.entranceAHeading + 90.0f;

  const float centerHeadingRad = centerHeading * DEG_TO_RAD_F;

  Point centerWorld;

  centerWorld.x = pose.entrance.x + cosf(centerHeadingRad) * radius;

  centerWorld.y = pose.entrance.y + sinf(centerHeadingRad) * radius;

  float startAngle =
      atan2f(pose.entrance.y - centerWorld.y, pose.entrance.x - centerWorld.x);

  float endAngle =
      atan2f(pose.exit.y - centerWorld.y, pose.exit.x - centerWorld.x);

  float delta = normalizeAngle(endAngle - startAngle);

  // Force the arc to follow the same direction as map.cpp.
  if (track.direction) {
    if (delta > 0.0f) {
      delta -= 2.0f * PI_F;
    }
  } else {
    if (delta < 0.0f) {
      delta += 2.0f * PI_F;
    }
  }

  const float radiusPixels = fabsf(radius * transform_.scale);

  int segments = static_cast<int>(fabsf(delta) * radiusPixels / 8.0f);

  if (segments < 8) {
    segments = 8;
  }

  if (segments > 96) {
    segments = 96;
  }

  Point previous = worldToMap({centerWorld.x + cosf(startAngle) * radius,

                               centerWorld.y + sinf(startAngle) * radius});

  // Dark outer track.
  for (int i = 1; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);

    const float angle = startAngle + delta * t;

    Point current = worldToMap({centerWorld.x + cosf(angle) * radius,

                                centerWorld.y + sinf(angle) * radius});

    drawSegment(layer, previous, current, config_.track, config_.trackWidth);

    previous = current;
  }

  // Light center line.
  previous = worldToMap({centerWorld.x + cosf(startAngle) * radius,

                         centerWorld.y + sinf(startAngle) * radius});

  for (int i = 1; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);

    const float angle = startAngle + delta * t;

    Point current = worldToMap({centerWorld.x + cosf(angle) * radius,

                                centerWorld.y + sinf(angle) * radius});

    drawSegment(layer, previous, current, config_.trackCenter,
                config_.centerWidth);

    previous = current;
  }
}

// ============================================================
// SWITCH
//
// map.cpp currently calculates:
//
//   entrance -> exit     = main route
//   entrance -> exitB    = branch
//
// entranceB is not populated by calculateSwitchPose(), so it is
// intentionally not rendered as another branch here.
// ============================================================

void LvTrackMap::drawSwitch(lv_layer_t* layer, const Track& track) const {
  drawStraight(layer, worldToMap(track.pose.entrance),
               worldToMap(track.pose.exit));

  drawStraight(layer, worldToMap(track.pose.entrance),
               worldToMap(track.pose.exitB));
}

// ============================================================
// CROSS
//
// map.cpp currently implements calculateCrossPose() by calling
// calculateStraightPose(), so there is currently only one
// calculated route in the canonical model.
// ============================================================

void LvTrackMap::drawCross(lv_layer_t* layer, const Track& track) const {
  drawStraight(layer, worldToMap(track.pose.entrance),
               worldToMap(track.pose.exit));
}

// ============================================================
// BUMPER
// ============================================================

void LvTrackMap::drawBumper(lv_layer_t* layer, const Track& track) const {
  const Point endpoint = worldToMap(track.pose.exit);

  const float heading = track.pose.exitAHeading * DEG_TO_RAD_F;

  // Perpendicular to the direction of travel.
  const float halfWidth = 15.0f * transform_.scale;

  const float dx = sinf(heading) * halfWidth;

  const float dy = -cosf(heading) * halfWidth;

  drawSegment(layer, {endpoint.x - dx, endpoint.y - dy},
              {endpoint.x + dx, endpoint.y + dy}, config_.bumper, 5);
}

// ============================================================
// TAGS
// ============================================================

void LvTrackMap::drawTags(lv_layer_t* layer, const Track& track) const {
  const Tag* tags[] = {&track.entranceA, &track.entranceB, &track.exitA,
                       &track.exitB};

  for (const Tag* tag : tags) {
    if (!tag) {
      continue;
    }

    // Don't draw an invalid/unassigned UID as a physical tag.
    if (!tag->uid.isValid()) {
      continue;
    }

    const Point world = tagWorldPosition(track, *tag);

    const Point point = worldToMap(world);

    const bool connected = tag->connectedTag != nullptr;

    drawCircle(layer, point, config_.tagRadius,
               connected ? config_.tagConnected : config_.tagUnconnected, true);

    const char* label = nullptr;

    if (tag == &track.entranceA) {
      label = "entranceA";
    } else if (tag == &track.entranceB) {
      label = "entranceB";
    } else if (tag == &track.exitA) {
      label = "exitA";
    } else if (tag == &track.exitB) {
      label = "exitB";
    }

    if (label) {
      drawText(layer, {point.x, point.y - 10.0f}, label, lv_color_hex(0x111111),
               true);
    }
  }
}

// ============================================================
// TRACK LABEL
// ============================================================

void LvTrackMap::drawLabel(lv_layer_t* layer, const Track& track,
                           size_t index) const {
  const Point a = worldToMap(track.pose.entrance);

  const Point b = worldToMap(track.pose.exit);

  Point center = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f - 12.0f};

  char text[64];

  snprintf(text, sizeof(text), "#%u %s", static_cast<unsigned>(index),
           trackTypeName(track.trackType));

  drawText(layer, center, text, config_.label, true);
}

// ============================================================
// BUTTON CALLBACKS
// ============================================================

void LvTrackMap::refreshButtonEvent(lv_event_t* e) {
  if (!e) {
    return;
  }

  LvTrackMap* map = static_cast<LvTrackMap*>(lv_event_get_user_data(e));

  if (map) {
    map->refresh();
  }
}

void LvTrackMap::resetButtonEvent(lv_event_t* e) {
  if (!e) {
    return;
  }

  LvTrackMap* map = static_cast<LvTrackMap*>(lv_event_get_user_data(e));

  if (map) {
    map->resetView();
  }
}
