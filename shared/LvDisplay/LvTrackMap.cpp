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
LvTrackMap::~LvTrackMap() { root_ = nullptr; }

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
  transformDirty_ = true;
  refresh();
}
Uid pastUid;
bool hasPastUid = false;
void addTrack(TrackMap* map, TrackType type, float length, float radius,
              float branchAngle, float branchRadius, float branchLength,
              bool direction, Uid entranceAUid, Uid exitAUid,
              Uid entranceBUid = Uid(), Uid exitBUid = Uid()) {
  map->createTrack(type, length, radius, branchAngle, branchRadius,
                   branchLength, direction, entranceAUid, exitAUid,
                   entranceBUid, exitBUid);

  if (hasPastUid) {
    Tag* entrance = map->findTagByUid(entranceAUid);
    Tag* previousExit = map->findTagByUid(pastUid);

    if (entrance && previousExit) {
      map->connectTags(entrance, previousExit);
    } else {
      Serial.println("ERROR: Could not find tags to connect");
    }
  }

  // Always remember this track's exit for the next track.
  pastUid = exitAUid;
  hasPastUid = true;
}
void LvTrackMap::setTrackMap(TrackMap* map) {
  if (!map) return;

  map_ = map;

  // Only do this if this function owns the test map.
  // map_->clearMap();

  Uid emptyUid;

  Uid entrance0("AA:AA:AA:AA:AA:AA:AA");
  Uid exit0("BB:BB:BB:BB:BB:BB:AA");

  Uid entrance1("AA:AA:AA:AA:AA:AA:11");
  Uid exit1("BB:BB:BB:BB:BB:BB:11");

  Uid entrance2("AA:AA:AA:AA:AA:AA:22");
  Uid exit2("BB:BB:BB:BB:BB:BB:22");

  Uid entrance3("AA:AA:AA:AA:AA:AA:33");
  Uid exit3("BB:BB:BB:BB:BB:BB:33");

  // Create tracks first.
  map_->createTrack(CURVED, 10, 10, 0, 0, 0, true, entrance0, exit0, emptyUid,
                    emptyUid);

  map_->createTrack(CURVED, 10, 10, 0, 0, 0, true, entrance1, exit1, emptyUid,
                    emptyUid);

  map_->createTrack(CURVED, 10, 10, 0, 0, 0, true, entrance2, exit2, emptyUid,
                    emptyUid);

  map_->createTrack(STRAIGHT, 10, 10, 0, 0, 0, true, entrance3, exit3, emptyUid,
                    emptyUid);

  // Connect tracks after they have all been created.
  auto connect = [&](const Uid& a, const Uid& b) {
    Tag* tagA = map_->findTagByUid(a);
    Tag* tagB = map_->findTagByUid(b);

    Serial.printf("Found tags: A=%p B=%p\n", static_cast<void*>(tagA),
                  static_cast<void*>(tagB));

    const bool success = map_->connectTags(tagA, tagB);

    Serial.printf("Connection successful: %s\n", success ? "YES" : "NO");

    if (success) {
      Serial.printf("Connection state: A=%d B=%d\n",
                    tagA->getConnectedTagUid().isValid(),
                    tagB->getConnectedTagUid().isValid());
    }
  };

  // Create a continuous chain.
  connect(entrance1, exit0);
  connect(entrance2, exit1);
  connect(entrance3, exit2);

  // Position the map.
  map_->calculateMap();

  JsonDocument doc;
  map_->mapToJson(doc);
  serializeJsonPretty(doc, Serial);

  transformDirty_ = true;
  refresh();
}
void LvTrackMap::clear() {
  map_ = nullptr;

  transform_ = {};

  refresh();
}

void LvTrackMap::invalidateMap() {
  transformDirty_ = true;
  refresh();
}

size_t LvTrackMap::trackCount() const {
  if (!map_) {
    Serial.println("no map");
    return 0;
  }

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

  if (width <= 0) {
    width = lv_obj_get_width(root_);
  }

  if (height <= 0) {
    height = lv_obj_get_height(root_);
  }

  if (width <= 0 || height <= 0) {
    return;
  }

  float minX = INFINITY;
  float minY = INFINITY;
  float maxX = -INFINITY;
  float maxY = -INFINITY;

  bool foundGeometry = false;

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

  constexpr int CURVE_SEGMENTS = 48;

  const uint8_t count = map_->getTrackCount();

  for (uint8_t i = 0; i < count; ++i) {
    const Track* track = map_->getTrack(i);

    if (!track || !track->getPositioned()) {
      continue;
    }

    const auto& points = track->getPoints();

    // Include all calculated track points.
    for (const Point& point : points) {
      includePoint(point.getX(), point.getY());
    }

    // Include the actual geometry of curved tracks.
    if (track->getTrackType() != CURVED || points.empty()) {
      continue;
    }

    const float radius = track->getRadius();
    const float length = track->getLength();

    if (radius <= 0.0f || length <= 0.0f) {
      continue;
    }

    const Point& entrance = points[0];

    const float direction = track->getDirection() ? -1.0f : 1.0f;

    const float heading = entrance.getHeading() * DEG_TO_RAD_F;

    const float centerHeading = heading + direction * (PI_F * 0.5f);

    const float centerX = entrance.getX() + cosf(centerHeading) * radius;

    const float centerY = entrance.getY() + sinf(centerHeading) * radius;

    const float startAngle =
        atan2f(entrance.getY() - centerY, entrance.getX() - centerX);

    const float delta = direction * (length / radius);

    // Sample the same curve that drawCurve() renders.
    for (int segment = 0; segment <= CURVE_SEGMENTS; ++segment) {
      const float t = static_cast<float>(segment) / CURVE_SEGMENTS;

      const float angle = startAngle + delta * t;

      includePoint(centerX + cosf(angle) * radius,
                   centerY + sinf(angle) * radius);
    }
  }

  if (!foundGeometry) {
    return;
  }

  const float worldWidth = fmaxf(maxX - minX, 1.0f);
  const float worldHeight = fmaxf(maxY - minY, 1.0f);

  // Leave enough screen space for track strokes and tags.
  const float padding = fmaxf(fmaxf(static_cast<float>(config_.padding), 12.0f),
                              static_cast<float>(config_.tagRadius) + 18.0f);

  const float availableWidth =
      fmaxf(static_cast<float>(width) - padding * 2.0f, 1.0f);

  const float availableHeight =
      fmaxf(static_cast<float>(height) - padding * 2.0f, 1.0f);

  // Uniform scale keeps the map's proportions.
  transform_.scale =
      fminf(availableWidth / worldWidth, availableHeight / worldHeight);

  if (!isfinite(transform_.scale) || transform_.scale <= 0.0f) {
    transform_.scale = 1.0f;
    return;
  }

  // Center the scaled map inside the available area.
  const float scaledWidth = worldWidth * transform_.scale;
  const float scaledHeight = worldHeight * transform_.scale;

  transform_.minX = minX;
  transform_.minY = minY;

  transform_.offsetX = padding + (availableWidth - scaledWidth) * 0.5f;

  transform_.offsetY = padding + (availableHeight - scaledHeight) * 0.5f;
}

void LvTrackMap::resetView() {
  calculateTransform();
  refresh();
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
  const bool resized = width != self->lastWidth_ || height != self->lastHeight_;

  if (self->transformDirty_ || resized) {
    self->calculateTransform(width, height);

    self->lastWidth_ = width;
    self->lastHeight_ = height;
    self->transformDirty_ = false;
  }

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

  // Coordinates are already screen-local.
  dsc.p1.x = static_cast<float>(rootArea.x1) + a.getX();
  dsc.p1.y = static_cast<float>(rootArea.y1) + a.getY();

  dsc.p2.x = static_cast<float>(rootArea.x1) + b.getX();
  dsc.p2.y = static_cast<float>(rootArea.y1) + b.getY();

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

    for (const Tag& tag : track->getTags()) {
      // No connection stored.
      if (!tag.getConnectedTagUid().isValid()) {
        continue;
      }

      const Tag* otherTag = map_->findTagByUid(tag.getConnectedTagUid());

      if (!otherTag) {
        continue;
      }
      const Track* otherTrack = map_->findTrackByUid(otherTag->getUid());

      if (!otherTrack || !otherTrack->getPositioned()) {
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
  const auto& tags = track.getTags();
  const auto& points = track.getPoints();

  for (size_t i = 0; i < tags.size() && i < points.size(); ++i) {
    if (&tag == &tags[i]) {
      return points[i];
    }
  }

  return {};
}

// ============================================================
// TRACK DRAWING
// ============================================================

void LvTrackMap::drawTrack(lv_layer_t* layer, const Track& track,
                           size_t index) const {
  if (!track.getPositioned()) {
    // Serial.println("track has not been positioned");

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

  const auto& points = track.getPoints();

  if (points.empty()) {
    return;
  }

  const float radius = track.getRadius();
  const float length = track.getLength();

  if (radius <= 0.0f || length <= 0.0f) {
    return;
  }

  const Point& entrance = points[0];

  const float direction = track.getDirection() ? -1.0f : 1.0f;

  const float delta = direction * (length / radius);

  const float heading = entrance.getHeading() * DEG_TO_RAD_F;

  const float centerHeading = heading + direction * (PI_F * 0.5f);

  const float centerX = entrance.getX() + cosf(centerHeading) * radius;

  const float centerY = entrance.getY() + sinf(centerHeading) * radius;

  const float startAngle =
      atan2f(entrance.getY() - centerY, entrance.getX() - centerX);

  Point previous = worldToMap(entrance);

  for (int i = 1; i <= SEGMENTS; ++i) {
    const float t = static_cast<float>(i) / SEGMENTS;
    const float angle = startAngle + delta * t;

    // Generate the point in world coordinates.
    Point worldPoint(centerX + cosf(angle) * radius,
                     centerY + sinf(angle) * radius, 0.0f);

    // Convert it into screen coordinates.
    Point current = worldToMap(worldPoint);

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
  static constexpr const char* labels[] = {"entranceA", "entranceB", "exitA",
                                           "exitB"};

  const auto& tags = track.getTags();

  for (size_t i = 0; i < tags.size(); ++i) {
    const Tag& tag = tags[i];

    if (!tag.getUid().isValid()) {
      continue;
    }

    const Point world = tagWorldPosition(track, tag);
    const Point point = worldToMap(world);

    const bool connected =
        map_->findTagByUid(tag.getConnectedTagUid()) != nullptr;

    drawCircle(layer, point, config_.tagRadius,
               connected ? config_.tagConnected : config_.tagUnconnected, true);

    if (i < sizeof(labels) / sizeof(labels[0])) {
      drawText(layer, {point.getX(), point.getY() - 10.0f, 0.0f}, labels[i],
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
