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

  // lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

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
  Uid emptyUid;
  map_->createTrack(CURVED, 10, 10, 0, 0, 0, true, emptyUid, emptyUid, emptyUid,
                    emptyUid);
  Serial.println("json doc in lvtrackmap");
  JsonDocument doc;
  map_->calculateMap();

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
    Serial.println("no map");
    return 0;
  }
  // Serial.print("track count ");
  // Serial.println(map_->getTrackCount());

  return map_->getTrackCount();
}

// ============================================================
// VIEW / TRANSFORM
// ============================================================
void LvTrackMap::calculateTransform(lv_coord_t width, lv_coord_t height) {
  transform_ = {};
  transform_.scale = 1.0f;

  if (!map_ || !root_) {
    return;
  }

  // If dimensions weren't explicitly supplied, get them here.
  if (width <= 0) {
    width = lv_obj_get_width(root_);
  }

  if (height <= 0) {
    height = lv_obj_get_height(root_);
  }

  if (width <= 0 || height <= 0) {
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

  // auto includePoint = [&](const Point& p) { includePoint(p.getX(), p.getY());
  // };

  auto includeRadius = [&](const Point& p, float radius) {
    includePoint(p.getX() - radius, p.getY() - radius);
    includePoint(p.getX() + radius, p.getY() + radius);
  };

  // ----------------------------------------------------------
  // Include a straight line and its physical width
  // ----------------------------------------------------------

  auto includeLine = [&](const Point& a, const Point& b, float halfWidth) {
    includePoint(a.getX(), a.getY());
    includePoint(b.getX(), b.getY());

    const float dx = b.getX() - a.getX();
    const float dy = b.getY() - a.getY();

    const float length = sqrtf(dx * dx + dy * dy);

    if (length > 0.0001f) {
      // Perpendicular unit vector.
      const float nx = -dy / length;
      const float ny = dx / length;

      includePoint(a.getX() + nx * halfWidth, a.getY() + ny * halfWidth);

      includePoint(a.getX() - nx * halfWidth, a.getY() - ny * halfWidth);

      includePoint(b.getX() + nx * halfWidth, b.getY() + ny * halfWidth);

      includePoint(b.getX() - nx * halfWidth, b.getY() - ny * halfWidth);
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
    includePoint(center.getX() + cosf(startAngle) * effectiveRadius,
                 center.getY() + sinf(startAngle) * effectiveRadius);

    const float endAngle = startAngle + delta;

    includePoint(center.getX() + cosf(endAngle) * effectiveRadius,
                 center.getY() + sinf(endAngle) * effectiveRadius);

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

      includePoint(center.getX() + cosf(angle) * effectiveRadius,
                   center.getY() + sinf(angle) * effectiveRadius);
    }
  };

  // ----------------------------------------------------------
  // Calculate geometry bounds
  // ----------------------------------------------------------

  for (uint8_t i = 0; i < count; ++i) {
    const Track* track = map_->getTrack(i);

    if (!track || !track->getPositioned()) {
      continue;
    }

    const std::vector<Point>& pose = track->getPoints();

    // --------------------------------------------------------
    // Track width in WORLD units.
    //
    // trackWidth is a screen-space rendering value, so we
    // don't use it here. Instead use a conservative physical
    // expansion based on the track geometry.
    // --------------------------------------------------------

    const float geometryPadding =
        0.5f *
        fmaxf(track->getRadius() > 0.0f ? track->getRadius() * 0.02f : 1.0f,
              1.0f);

    switch (track->getTrackType()) {
        // ======================================================
        // STRAIGHT
        // ======================================================

      case STRAIGHT: {
        includeLine(pose[0], pose[1], geometryPadding);

        break;
      }

        // ======================================================
        // CURVED
        // ======================================================

      case CURVED: {
        if (track->getRadius() <= 0.0f) {
          includeLine(pose[0], pose[1], geometryPadding);

          break;
        }

        const float radius = track->getRadius();

        // map.cpp uses:
        //
        // true  -> heading - 90
        // false -> heading + 90
        //

        const float centerHeading = track->getDirection()
                                        ? pose[0].getHeading() - 90.0f
                                        : pose[0].getHeading() + 90.0f;

        const float centerHeadingRad = centerHeading * DEG_TO_RAD_F;

        Point centerWorld;

        centerWorld.setX(pose[0].getX() + cosf(centerHeadingRad) * radius);

        centerWorld.setY(pose[0].getY() + sinf(centerHeadingRad) * radius);

        const float startAngle = atan2f(pose[0].getY() - centerWorld.getY(),
                                        pose[0].getX() - centerWorld.getX());

        const float endAngle = atan2f(pose[1].getY() - centerWorld.getY(),
                                      pose[1].getX() - centerWorld.getX());

        float delta = normalizeAngle(endAngle - startAngle);

        // Match drawCurve().
        if (track->getDirection()) {
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
        includeLine(pose[0], pose[1], geometryPadding);

        includeLine(pose[0], pose[3], geometryPadding);

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
        includeLine(pose[0], pose[1], geometryPadding);

        break;
      }

        // ======================================================
        // BUMPER
        // ======================================================

      case BUMPER: {
        includeLine(pose[0], pose[1], geometryPadding);

        // Include the bumper itself.
        //
        // drawBumper() currently uses a 15 pixel half-width,
        // but this is screen-space. Use a conservative world
        // expansion here.
        const float bumperWorldRadius = fmaxf(geometryPadding, 1.0f);

        includeRadius(pose[1], bumperWorldRadius);

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

    for (const Tag tag : track->getTags()) {
      if (!tag.getUid().isValid()) {
        continue;
      }

      const Point tagPosition = tagWorldPosition(*track, tag);

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

  // Serial.println("=== Track Map Transform ===");

  // Serial.print("Bounds X: ");
  // Serial.print(minX);
  // Serial.print(" -> ");
  // Serial.println(maxX);

  // Serial.print("Bounds Y: ");
  // Serial.print(minY);
  // Serial.print(" -> ");
  // Serial.println(maxY);

  // Serial.print("Geometry size: ");
  // Serial.print(worldWidth);
  // Serial.print(" x ");
  // Serial.println(worldHeight);

  // Serial.print("Display size: ");
  // Serial.print(width);
  // Serial.print(" x ");
  // Serial.println(height);

  // Serial.print("Scale: ");
  // Serial.println(transform_.scale);

  // Serial.print("Offset: ");
  // Serial.print(offsetX);
  // Serial.print(", ");
  // Serial.println(offsetY);
}
void LvTrackMap::resetView() {
  calculateTransform();
  refresh();
}

std::vector<LvTrackMap::Point> LvTrackMap::worldToMap(
    std::vector<Point> points) const {
  std::vector<LvTrackMap::Point> result;
  result.reserve(points.size());  // Optimizes allocation

  for (const auto& point : points) {
    // Loop through each point and apply the transform to it individually
    auto mapped_point = worldToMap(point);

    result.push_back(mapped_point);
  }

  return result;
}

LvTrackMap::Point LvTrackMap::worldToMap(Point point) const {
  Point result;

  result.setX((point.getX() - transform_.minX) * transform_.scale +
              transform_.offsetX);

  result.setY((point.getY() - transform_.minY) * transform_.scale +
              transform_.offsetY);

  result.setHeading(point.getHeading());

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

  const lv_coord_t width = lv_area_get_width(&area);

  const lv_coord_t height = lv_area_get_height(&area);

  if (width <= 0 || height <= 0) {
    return;
  }

  // IMPORTANT:
  // Recalculate after LVGL has established the
  // real size of the object.
  self->calculateTransform();

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
  // TODO check angle

  self->drawText(layer, {static_cast<float>(width) / 2.0f, 10.0f, 0.0f},
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
    drawSegment(layer, {static_cast<float>(x), 0.0f, 0.0f},
                {static_cast<float>(x), static_cast<float>(height), 0.0f},
                config_.grid, 1);
  }

  for (int32_t y = 0; y < height; y += spacing) {
    drawSegment(layer, {0.0f, static_cast<float>(y), 0.0f},
                {static_cast<float>(width), static_cast<float>(y), 0.0f},
                config_.grid, 1);
  }
}

// ============================================================
// LOW-LEVEL DRAW HELPERS
// ============================================================

void LvTrackMap::drawSegment(lv_layer_t* layer, Point a, Point b,
                             lv_color_t color, uint8_t width) const {
  if (!layer || !root_) {
    return;
  }

  lv_draw_line_dsc_t dsc;
  lv_draw_line_dsc_init(&dsc);

  dsc.color = color;
  dsc.width = (width > 0) ? width : 1;
  dsc.opa = LV_OPA_COVER;
  dsc.round_start = true;
  dsc.round_end = true;

  lv_area_t rootArea;
  lv_obj_get_coords(root_, &rootArea);

  const float scale = transform_.scale;
  const float offsetX = transform_.offsetX;
  const float offsetY = transform_.offsetY;

  auto transformPoint = [&](const Point& point) -> lv_point_precise_t {
    lv_point_precise_t result;

    result.x = rootArea.x1 + offsetX + point.getX() * scale;
    result.y = rootArea.y1 + offsetY + point.getY() * scale;

    return result;
  };

  dsc.p1 = transformPoint(a);
  dsc.p2 = transformPoint(b);

  lv_draw_line(layer, &dsc);
}
void LvTrackMap::drawCircle(lv_layer_t* layer, Point center, int32_t radius,
                            lv_color_t color, bool filled) const {
  // Serial.println("draw circle");

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

  area.x1 =
      rootArea.x1 + static_cast<lv_coord_t>(lroundf(center.getX() - radius));

  area.y1 =
      rootArea.y1 + static_cast<lv_coord_t>(lroundf(center.getY() - radius));

  area.x2 =
      rootArea.x1 + static_cast<lv_coord_t>(lroundf(center.getX() + radius));

  area.y2 =
      rootArea.y1 + static_cast<lv_coord_t>(lroundf(center.getY() + radius));

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

  area.x1 =
      rootArea.x1 + static_cast<lv_coord_t>(lroundf(center.getX() - 100.0f));

  area.x2 =
      rootArea.x1 + static_cast<lv_coord_t>(lroundf(center.getX() + 100.0f));

  area.y1 =
      rootArea.y1 + static_cast<lv_coord_t>(lroundf(center.getY() - 8.0f));

  area.y2 =
      rootArea.y1 + static_cast<lv_coord_t>(lroundf(center.getY() + 8.0f));

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

    if (!track || !track->getPositioned()) {
      continue;
    }

    for (Tag tag : track->getTags()) {
      // No connection stored.
      if (!tag.getConnectedTagUid().isValid()) {
        continue;
      }

      const Tag* otherTag = map_->findTagByUid(tag.getConnectedTagUid());

      if (!otherTag) {
        continue;
      }
      const Track* otherTrack = map_->findTrackByUid(otherTag->getUid());

      if (!otherTrack || otherTrack->getPositioned()) {
        continue;
      }

      const int otherIndex = map_->trackIndex(otherTrack);

      if (otherIndex < 0) {
        continue;
      }

      // Connection exists in both directions.
      // Only draw it once.
      if (i > static_cast<uint8_t>(otherIndex)) {
        continue;
      }

      const Point aWorld = tagWorldPosition(*track, tag);

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

LvTrackMap::Point LvTrackMap::tagWorldPosition(const Track& track,
                                               const Tag& tag) const {
  // TODO have to check order? might be ok
  // TODO turn into for loop
  if (&tag == &track.getTags()[0]) {
    return track.getPoints()[0];
  }

  if (&tag == &track.getTags()[1]) {
    return track.getPoints()[1];
  }

  if (&tag == &track.getTags()[2]) {
    return track.getPoints()[2];
  }

  if (&tag == &track.getTags()[3]) {
    return track.getPoints()[3];
  }

  return {};
}

// ============================================================
// TRACK DRAWING
// ============================================================

void LvTrackMap::drawTrack(lv_layer_t* layer, const Track& track,
                           size_t index) const {
  // Serial.println("draw track");

  if (!track.getPositioned()) {
    Serial.println("track has not been positioned");

    return;
  }

  switch (track.getTrackType()) {
    case STRAIGHT:
      drawStraight(layer, worldToMap(track.getPoints()[0]),
                   worldToMap(track.getPoints()[1]));
      break;

    case CURVED:
      drawCurve(layer, track);

      // Draw bumper at exit point for curved tracks.
      // drawBumper(layer, track);
      break;

    case SWITCH:
      drawSwitch(layer, track);
      break;

    case CROSS:
      drawCross(layer, track);
      break;

    case BUMPER:
      drawStraight(layer, worldToMap(track.getPoints()[0]),
                   worldToMap(track.getPoints()[1]));

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
  drawSegment(layer, a, b, lv_color_hex(0xFF0000), 10);

  drawSegment(layer, a, b, lv_color_hex(0X00FF00), 4);
}

// ============================================================
// CURVE
//
// IMPORTANT:
// This function only renders the curve.
//
// It does NOT calculate or modify track.getPoints().
//
// map.cpp is the source of truth for the curve endpoints and headings.
// ============================================================
void LvTrackMap::drawCurve(lv_layer_t* layer, const Track& track) const {
  constexpr int SEGMENTS = 48;

  const auto& points = worldToMap(track.getPoints());

  const float radius = track.getRadius();
  const float length = track.getLength();

  if (radius <= 0.0f || length <= 0.0f) {
    Serial.printf("ERROR: Invalid curve dimensions. Radius=%.2f Length=%.2f\n",
                  radius, length);
    return;
  }

  const Point& entrance = points[0];

  // Calculate signed angular sweep.
  const float direction = track.getDirection() ? -1.0f : 1.0f;
  const float delta = direction * (length / radius);

  // Calculate curve center.
  const float headingRadians = entrance.getHeading() * (M_PI / 180.0f);

  const float centerHeading = headingRadians + direction * (M_PI / 2.0f);

  const float centerX = entrance.getX() + cosf(centerHeading) * radius;

  const float centerY = entrance.getY() + sinf(centerHeading) * radius;

  // Starting angle from center to entrance.
  const float startAngle =
      atan2f(entrance.getY() - centerY, entrance.getX() - centerX);

  // Serial.println("========== DRAW CURVE ==========");
  // Serial.printf("Entrance: X=%.2f Y=%.2f Heading=%.2f\n", entrance.getX(),
  //               entrance.getY(), entrance.getHeading());
  // Serial.printf("Length: %.2f Radius: %.2f\n", length, radius);
  // Serial.printf("Direction: %d\n", track.getDirection());
  // Serial.printf("Center: X=%.2f Y=%.2f\n", centerX, centerY);
  // Serial.printf("Start angle: %.4f Delta: %.4f\n", startAngle, delta);

  Point previous = entrance;

  for (int i = 1; i <= SEGMENTS; ++i) {
    const float t = static_cast<float>(i) / SEGMENTS;
    const float angle = startAngle + delta * t;

    Point current(centerX + cosf(angle) * radius,
                  centerY + sinf(angle) * radius, 0.0f);
    // const float endAngle = startAngle + delta;
    // Serial.printf("Expected exit: X=%.2f Y=%.2f\n",
    //               centerX + cosf(endAngle) * radius,
    //               centerY + sinf(endAngle) * radius);
    // if (i == 1 || i == SEGMENTS / 2 || i == SEGMENTS) {
    //   Serial.printf("Segment %d: (%.2f, %.2f) -> (%.2f, %.2f)\n", i,
    //                 previous.getX(), previous.getY(), current.getX(),
    //                 current.getY());
    // }
    drawSegment(layer, previous, current, lv_color_hex(0x00FF00), 4);
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
  drawStraight(layer, worldToMap(track.getPoints()[0]),
               worldToMap(track.getPoints()[1]));

  drawStraight(layer, worldToMap(track.getPoints()[0]),
               worldToMap(track.getPoints()[3]));
}

// ============================================================
// CROSS
//
// map.cpp currently implements calculateCrossPose() by calling
// calculateStraightPose(), so there is currently only one
// calculated route in the canonical model.
// ============================================================

void LvTrackMap::drawCross(lv_layer_t* layer, const Track& track) const {
  drawStraight(layer, worldToMap(track.getPoints()[0]),
               worldToMap(track.getPoints()[1]));
}

// ============================================================
// BUMPER
// ============================================================

void LvTrackMap::drawBumper(lv_layer_t* layer, const Track& track) const {
  const Point endpoint = worldToMap(track.getPoints()[1]);

  const float heading = endpoint.getHeading() * DEG_TO_RAD_F;

  // Perpendicular to the direction of travel.
  const float halfWidth = 15.0f * transform_.scale;

  const float dx = sinf(heading) * halfWidth;

  const float dy = -cosf(heading) * halfWidth;
  // TODO check angle

  drawSegment(
      layer,
      {endpoint.getX() - dx, endpoint.getY() - dy, endpoint.getHeading()},
      {endpoint.getX() + dx, endpoint.getY() + dy, endpoint.getHeading()},
      config_.bumper, 5);
}

// ============================================================
// TAGS
// ============================================================

void LvTrackMap::drawTags(lv_layer_t* layer, const Track& track) const {
  for (const Tag tag : track.getTags()) {
    // Don't draw an invalid/unassigned UID as a physical tag.
    if (!tag.getUid().isValid()) {
      continue;
    }

    const Point world = tagWorldPosition(track, tag);

    const Point point = worldToMap(world);

    const bool connected =
        map_->findTagByUid(tag.getConnectedTagUid()) != nullptr;

    drawCircle(layer, point, config_.tagRadius,
               connected ? config_.tagConnected : config_.tagUnconnected, true);

    const char* label = nullptr;

    if (tag == track.getTags()[0]) {
      label = "entranceA";
    } else if (tag == track.getTags()[1]) {
      label = "entranceB";
    } else if (tag == track.getTags()[2]) {
      label = "exitA";
    } else if (tag == track.getTags()[3]) {
      label = "exitB";
    }

    if (label) {
      // TODO check angle

      drawText(layer, {point.getX(), point.getY() - 10.0f, 0.0f}, label,
               lv_color_hex(0x111111), true);
    }
  }
}

// ============================================================
// TRACK LABEL
// ============================================================

void LvTrackMap::drawLabel(lv_layer_t* layer, const Track& track,
                           size_t index) const {
  const Point a = worldToMap(track.getPoints()[0]);

  const Point b = worldToMap(track.getPoints()[1]);
  // todo check angle
  Point center = {(a.getX() + b.getX()) * 0.5f,
                  (a.getY() + b.getY()) * 0.5f - 12.0f, 0.0f};

  char text[64];

  snprintf(text, sizeof(text), "#%u %s", static_cast<unsigned>(index),
           track.trackTypeToString());

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
