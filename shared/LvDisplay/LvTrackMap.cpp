#include "LvTrackMap.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
// LvTrackMap trackMapDisplay(screen);
#include "../track/map.h"

namespace {

constexpr float PI_F = 3.14159265358979323846f;
constexpr float DEG_TO_RAD_F = PI_F / 180.0f;

//
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

// LvTrackMap::LvTrackMap(lv_obj_t* parent) {
//   root_ = lv_obj_create(parent);

//   lv_obj_remove_style_all(root_);
//   lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
//   //   lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
//   lv_obj_set_scroll_dir(root_, LV_DIR_NONE);

//   installCallbacks();
// }
LvTrackMap::LvTrackMap(lv_obj_t* parent) {
  Serial.println("LvTrackMap constructor");

  root_ = lv_obj_create(parent);

  if (!root_) {
    Serial.println("FAILED creating root");
    return;
  }

  Serial.println("root created");

  lv_obj_remove_style_all(root_);

  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(root_, 0, 0);

  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  installCallbacks();

  Serial.println("LvTrackMap constructor finished");
}
LvTrackMap::~LvTrackMap() {
  // root_ is owned by LVGL/the parent object.
  // Do not delete it here.
  root_ = nullptr;
}

// void LvTrackMap::installCallbacks() {
//   if (!root_) {
//     return;
//   }

//   lv_obj_add_event_cb(root_, drawEvent, LV_EVENT_DRAW_MAIN, this);
// }

void LvTrackMap::installCallbacks() {
  Serial.println("installCallbacks()");

  if (!root_) {
    Serial.println("root_ is NULL");
    return;
  }

  Serial.println("adding DRAW_MAIN callback");

  lv_obj_add_event_cb(root_, LvTrackMap::drawEvent, LV_EVENT_DRAW_MAIN, this);

  Serial.println("DRAW_MAIN callback added");
}
void LvTrackMap::setConfig(const Config& config) {
  config_ = config;
  calculateTransform();
  refresh();
}

void LvTrackMap::setTrackMap(TrackMap* map) {
  map_ = map;
  Serial.println("json doc in lvtrackmap");
  JsonDocument doc;
  map_->mapToJson(doc);
  serializeJsonPretty(doc, Serial);
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

  if (!map_ || !root_) {
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

  bool foundGeometry = false;

  // ----------------------------------------------------------
  // Helpers
  // ----------------------------------------------------------

  auto includePoint = [&](float x, float y) {
    if (!isfinite(x) || !isfinite(y)) {
      return;
    }

    minX = fminf(minX, x);
    minY = fminf(minY, y);
    maxX = fmaxf(maxX, x);
    maxY = fmaxf(maxY, y);

    foundGeometry = true;
  };

  // auto includePoint = [&](const Point& p) { includePoint(p.x, p.y); };

  auto includeRadius = [&](const Point& p, float radius) {
    includePoint(p.x - radius, p.y - radius);
    includePoint(p.x + radius, p.y + radius);
  };

  // ----------------------------------------------------------
  // Include a straight line and its physical width
  // ----------------------------------------------------------

  auto includeLine = [&](const Point& a, const Point& b, float halfWidth) {
    includePoint(a.x, a.y);
    includePoint(b.x, b.y);

    const float dx = b.x - a.x;
    const float dy = b.y - a.y;

    const float length = sqrtf(dx * dx + dy * dy);

    if (length > 0.0001f) {
      // Perpendicular unit vector.
      const float nx = -dy / length;
      const float ny = dx / length;

      includePoint(a.x + nx * halfWidth, a.y + ny * halfWidth);

      includePoint(a.x - nx * halfWidth, a.y - ny * halfWidth);

      includePoint(b.x + nx * halfWidth, b.y + ny * halfWidth);

      includePoint(b.x - nx * halfWidth, b.y - ny * halfWidth);
    } else {
      includeRadius(a, halfWidth);
    }
  };

  // ----------------------------------------------------------
  // Include a circular arc
  //
  // This checks the start/end angles plus every quadrant
  // where the circle can reach its X/Y extrema.
  // ----------------------------------------------------------

  auto includeArc = [&](const Point& center, float radius, float startAngle,
                        float delta, float halfWidth) {
    if (!isfinite(radius) || radius <= 0.0f) {
      return;
    }

    const float effectiveRadius = radius + halfWidth;

    // Always include the endpoints.
    includePoint(center.x + cosf(startAngle) * effectiveRadius,
                 center.y + sinf(startAngle) * effectiveRadius);

    const float endAngle = startAngle + delta;

    includePoint(center.x + cosf(endAngle) * effectiveRadius,
                 center.y + sinf(endAngle) * effectiveRadius);

    // --------------------------------------------------------
    // Test the four circle extrema:
    //
    // 0
    // PI/2
    // PI
    // 3PI/2
    //
    // Only include them if they fall within the arc.
    // --------------------------------------------------------

    auto angleOnArc = [&](float angle) {
      const float twoPi = 2.0f * PI_F;

      float relative = angle - startAngle;

      if (delta >= 0.0f) {
        while (relative < 0.0f) {
          relative += twoPi;
        }

        while (relative > twoPi) {
          relative -= twoPi;
        }

        return relative <= delta + 0.0001f;
      } else {
        while (relative > 0.0f) {
          relative -= twoPi;
        }

        while (relative < -twoPi) {
          relative += twoPi;
        }

        return relative >= delta - 0.0001f;
      }
    };

    const float extrema[] = {
        0.0f,
        PI_F * 0.5f,
        PI_F,
        PI_F * 1.5f,
    };

    for (float angle : extrema) {
      if (!angleOnArc(angle)) {
        continue;
      }

      includePoint(center.x + cosf(angle) * effectiveRadius,
                   center.y + sinf(angle) * effectiveRadius);
    }
  };

  // ----------------------------------------------------------
  // Calculate geometry bounds
  // ----------------------------------------------------------

  for (uint8_t i = 0; i < count; ++i) {
    const Track* track = map_->getTrack(i);

    if (!track || !track->positioned) {
      continue;
    }

    const TrackPose& pose = track->pose;

    // --------------------------------------------------------
    // Track width in WORLD units.
    //
    // trackWidth is a screen-space rendering value, so we
    // don't use it here. Instead use a conservative physical
    // expansion based on the track geometry.
    // --------------------------------------------------------

    const float geometryPadding =
        0.5f * fmaxf(track->radius > 0.0f ? track->radius * 0.02f : 1.0f, 1.0f);

    switch (track->trackType) {
        // ======================================================
        // STRAIGHT
        // ======================================================

      case STRAIGHT: {
        includeLine(pose.entrance, pose.exit, geometryPadding);

        break;
      }

        // ======================================================
        // CURVED
        // ======================================================

      case CURVED: {
        if (track->radius <= 0.0f) {
          includeLine(pose.entrance, pose.exit, geometryPadding);

          break;
        }

        const float radius = track->radius;

        // map.cpp uses:
        //
        // true  -> heading - 90
        // false -> heading + 90
        //

        const float centerHeading = track->direction
                                        ? pose.entranceAHeading - 90.0f
                                        : pose.entranceAHeading + 90.0f;

        const float centerHeadingRad = centerHeading * DEG_TO_RAD_F;

        Point centerWorld;

        centerWorld.x = pose.entrance.x + cosf(centerHeadingRad) * radius;

        centerWorld.y = pose.entrance.y + sinf(centerHeadingRad) * radius;

        const float startAngle = atan2f(pose.entrance.y - centerWorld.y,
                                        pose.entrance.x - centerWorld.x);

        const float endAngle =
            atan2f(pose.exit.y - centerWorld.y, pose.exit.x - centerWorld.x);

        float delta = normalizeAngle(endAngle - startAngle);

        // Match drawCurve().
        if (track->direction) {
          if (delta > 0.0f) {
            delta -= 2.0f * PI_F;
          }
        } else {
          if (delta < 0.0f) {
            delta += 2.0f * PI_F;
          }
        }

        includeArc(centerWorld, radius, startAngle, delta, geometryPadding);

        break;
      }

        // ======================================================
        // SWITCH
        // ======================================================

      case SWITCH: {
        includeLine(pose.entrance, pose.exit, geometryPadding);

        includeLine(pose.entrance, pose.exitB, geometryPadding);

        break;
      }

        // ======================================================
        // CROSS
        // ======================================================

      case CROSS: {
        // Your current drawCross() only renders entrance -> exit.
        //
        // If you later add the second crossing route, add it
        // here too.
        includeLine(pose.entrance, pose.exit, geometryPadding);

        break;
      }

        // ======================================================
        // BUMPER
        // ======================================================

      case BUMPER: {
        includeLine(pose.entrance, pose.exit, geometryPadding);

        // Include the bumper itself.
        //
        // drawBumper() currently uses a 15 pixel half-width,
        // but this is screen-space. Use a conservative world
        // expansion here.
        const float bumperWorldRadius = fmaxf(geometryPadding, 1.0f);

        includeRadius(pose.exit, bumperWorldRadius);

        break;
      }

      default:
        break;
    }

    // --------------------------------------------------------
    // Include TAG geometry.
    //
    // Tags are rendered as circles and labels. Include the
    // physical tag radius so they don't get clipped.
    // --------------------------------------------------------

    const Tag* tags[] = {
        &track->entranceA,
        &track->entranceB,
        &track->exitA,
        &track->exitB,
    };

    for (const Tag* tag : tags) {
      if (!tag || !tag->uid.isValid()) {
        continue;
      }

      const Point tagPosition = tagWorldPosition(*track, *tag);

      // Conservative world-space tag radius.
      //
      // Since tagRadius is screen-space, use a small
      // geometry expansion here. The final margin below
      // protects the actual rendered circle.
      includeRadius(tagPosition, geometryPadding);
    }
  }

  // ----------------------------------------------------------
  // Validate bounds
  // ----------------------------------------------------------

  if (!foundGeometry || !isfinite(minX) || !isfinite(minY) || !isfinite(maxX) ||
      !isfinite(maxY)) {
    return;
  }

  // ----------------------------------------------------------
  // Add a world-space margin.
  //
  // This prevents antialiased lines / labels / tags from
  // touching the edge after scaling.
  // ----------------------------------------------------------

  float worldWidth = maxX - minX;

  float worldHeight = maxY - minY;

  if (worldWidth < 0.001f) {
    worldWidth = 1.0f;
  }

  if (worldHeight < 0.001f) {
    worldHeight = 1.0f;
  }

  const float worldMargin = fmaxf(fmaxf(worldWidth, worldHeight) * 0.05f, 1.0f);

  minX -= worldMargin;
  maxX += worldMargin;
  minY -= worldMargin;
  maxY += worldMargin;

  worldWidth = maxX - minX;

  worldHeight = maxY - minY;

  // ----------------------------------------------------------
  // Get actual LVGL object dimensions
  // ----------------------------------------------------------

  lv_coord_t width = lv_obj_get_width(root_);

  lv_coord_t height = lv_obj_get_height(root_);

  if (width <= 0) {
    width = config_.width;
  }

  if (height <= 0) {
    height = config_.height;
  }

  if (width <= 0 || height <= 0) {
    return;
  }

  const float padding = static_cast<float>(config_.padding);

  const float availableWidth =
      fmaxf(static_cast<float>(width) - padding * 2.0f, 1.0f);

  const float availableHeight =
      fmaxf(static_cast<float>(height) - padding * 2.0f, 1.0f);

  // ----------------------------------------------------------
  // Uniform scale
  //
  // This makes the ENTIRE geometry fit.
  // ----------------------------------------------------------

  transform_.scale =
      fminf(availableWidth / worldWidth, availableHeight / worldHeight);

  if (!isfinite(transform_.scale) || transform_.scale <= 0.0f) {
    transform_.scale = 1.0f;
  }

  // ----------------------------------------------------------
  // Center the complete map inside the display.
  // ----------------------------------------------------------

  const float scaledWidth = worldWidth * transform_.scale;

  const float scaledHeight = worldHeight * transform_.scale;

  const float offsetX = (static_cast<float>(width) - scaledWidth) * 0.5f;

  const float offsetY = (static_cast<float>(height) - scaledHeight) * 0.5f;

  transform_.minX = minX;
  transform_.minY = minY;

  transform_.offsetX = offsetX;
  transform_.offsetY = offsetY;

  // ----------------------------------------------------------
  // Debug
  // ----------------------------------------------------------

  Serial.println("=== Track Map Transform ===");

  Serial.print("Bounds X: ");
  Serial.print(minX);
  Serial.print(" -> ");
  Serial.println(maxX);

  Serial.print("Bounds Y: ");
  Serial.print(minY);
  Serial.print(" -> ");
  Serial.println(maxY);

  Serial.print("Geometry size: ");
  Serial.print(worldWidth);
  Serial.print(" x ");
  Serial.println(worldHeight);

  Serial.print("Display size: ");
  Serial.print(width);
  Serial.print(" x ");
  Serial.println(height);

  Serial.print("Scale: ");
  Serial.println(transform_.scale);

  Serial.print("Offset: ");
  Serial.print(offsetX);
  Serial.print(", ");
  Serial.println(offsetY);
}
void LvTrackMap::resetView() {
  calculateTransform();
  refresh();
}
LvTrackMap::Point LvTrackMap::worldToMap(Point point) const {
  Point result;

  result.x =
      (point.x - transform_.minX) * transform_.scale + transform_.offsetX;

  result.y =
      (point.y - transform_.minY) * transform_.scale + transform_.offsetY;

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
  Serial.println("draw event ");
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

  // resetView();  //  TODO diff solution just temp
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
  Serial.println("draw circle");

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
  Serial.println("draw connections");

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
      if (!tag || !map_->findTagByUid(tag->connectedTagUid)) {
        continue;
      }

      const Tag* otherTag = map_->findTagByUid(tag->connectedTagUid);

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
  Serial.println("draw track");

  if (!track.positioned) {
    Serial.println("track has not been positioned");

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
  Serial.println("draw curve");

  if (track.radius <= 0.0f) {
    drawStraight(layer, worldToMap(pose.entrance), worldToMap(pose.exit));

    return;
  }

  const float radius = track.radius;
  Serial.println("radius");

  Serial.println(radius);

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

    const bool connected = map_->findTagByUid(tag->connectedTagUid) != nullptr;

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
           map_->trackTypeToString(track.trackType));

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
