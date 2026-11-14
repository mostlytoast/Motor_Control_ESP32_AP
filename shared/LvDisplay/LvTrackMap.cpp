#include "LvTrackMap.h"

namespace {

constexpr float DEG_TO_RAD_F = M_PI / 180.0f;

//
static float normalizeAngle(float angle) {
  while (angle > M_PI) {
    angle -= 2.0f * M_PI;
  }

  while (angle < -M_PI) {
    angle += 2.0f * M_PI;
  }

  return angle;
}

}  // namespace

// ============================================================
// CONSTRUCTION / CONFIGURATION
// ============================================================

LvTrackMap::LvTrackMap(lv_obj_t* parent,
                       std::function<void()> sendStateToReceiverCallback_) {
  sendStateToReceiverCallback = sendStateToReceiverCallback_;

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

  lv_obj_set_scrollable(root_, false);

  installCallbacks();

  hide_button_ = display_.createActionButton(
      root_, "Hide", 110, 27, 100, 25, [this]() { toggle(); }, kColorForward);

  auto_btn_ = display_.createActionButton(
      root_, "Auto", 215, 27, 100, 25, [this]() { togglePathFind(); },
      kColorForward);
  auto_label_ = display_.createLabel(root_, kColorStatus, 8, 53, LV_PART_MAIN,
                                     LV_ALIGN_TOP_LEFT);

  hitBoxLayer_ = lv_obj_create(root_);

  if (!hitBoxLayer_) {
    Serial.println("FAILED creating hitBoxLayer");
    return;
  }
  lv_obj_remove_style_all(hitBoxLayer_);

  lv_obj_set_size(hitBoxLayer_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(hitBoxLayer_, 0, 0);
  lv_obj_set_scrollable(hitBoxLayer_, false);
  lv_obj_set_clickable(hitBoxLayer_, false);

  // lv_obj_set_scrollable(hitBoxLayer_, false);
  lv_obj_set_hidden(root_, true);

  lv_obj_move_foreground(hide_button_);
  lv_obj_move_foreground(auto_btn_);
  lv_obj_move_foreground(auto_label_);

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
  Serial.println("track map called");
  map_ = map;
  // JsonDocument doc;
  // deserializeJson(doc,
  //                 "{\"l\": 14.13717,\"r\": 18,\"d\": true,\"a\": 0,\"h\": "
  //                 "0,\"o\": 0,\"t\": 1,\"g\": [{\"uid\": "
  //                 "\"04:23:9B:9C:E4:2A:81\",\"internalConnectedTagUids\": "
  //                 "[\"04:2E:9B:9C:E4:2A:81\"]},{\"uid\": "
  //                 "\"04:2E:9B:9C:E4:2A:81\",\"internalConnectedTagUids\": "
  //                 "[\"04:23:9B:9C:E4:2A:81\"]},{\"uid\": "
  //                 "\"\",\"internalConnectedTagUids\": []},{\"uid\": "
  //                 "\"\",\"internalConnectedTagUids\": []}]}");

  // bool success;
  // Track track = Track(success, doc.as<JsonObject>(), true);
  // Serial.println(success);
  // // JsonObject printDoc;
  // // track.toCondensedJson(printDoc);
  // // serializeJsonPretty(printDoc, Serial);
  // if (success) {
  //   track.printTrack();
  // }
  // map->addTrack(track);
  // JsonDocument doc2;
  // deserializeJson(doc2,
  //                 "{\"l\": 10,\"r\": 0,\"d\": true,\"a\": 0,\"h\": 0,\"o\": "
  //                 "0,\"t\": 1,\"g\": [{\"uid\": "
  //                 "\"04:E2:9A:9C:E4:2A:81\",\"internalConnectedTagUids\": "
  //                 "[\"04:E1:9A:9C:E4:2A:81\"]},{\"uid\": "
  //                 "\"04:E1:9A:9C:E4:2A:81\",\"internalConnectedTagUids\": "
  //                 "[\"04:E2:9A:9C:E4:2A:81\"]},{\"uid\": "
  //                 "\"\",\"internalConnectedTagUids\": []},{\"uid\": "
  //                 "\"\",\"internalConnectedTagUids\": []}]}");

  // bool success2;
  // Track track2 = Track(success2, doc2.as<JsonObject>(), true);
  // map->addTrack(track2);

  // Serial.println(success2);
  // // JsonObject printDoc2;
  // // track2.toCondensedJson(printDoc2);
  // // serializeJsonPretty(printDoc2, Serial);
  // if (success2) {
  //   track2.printTrack();
  // }

  // Uid emptyUid;

  // Uid entrance0("AA:AA:AA:AA:AA:AA:AA");
  // Uid exit0("BB:BB:BB:BB:BB:BB:AA");

  // Uid entrance1("AA:AA:AA:AA:AA:AA:11");
  // Uid exit1("BB:BB:BB:BB:BB:BB:11");

  // Uid entrance2("AA:AA:AA:AA:AA:AA:22");
  // Uid exit2("BB:BB:BB:BB:BB:BB:22");
  // Uid entrance2B("AA:AA:AA:AA:AA:BB:22");
  // Uid exit2B("BB:BB:BB:BB:BB:CC:22");

  // Uid entrance3("AA:AA:AA:AA:AA:AA:33");
  // Uid exit3("BB:BB:BB:BB:BB:BB:33");
  // Serial.println("starting tracks setup");

  // // Create tracks first.
  // map_->createTrack(STRAIGHT, 10, 0, 0, 0, 0, true, entrance0, exit0,
  // emptyUid,
  //                   emptyUid);

  // map_->createTrack(CURVED, 10, 10, 0, 0, 0, true, entrance1, exit1,
  // emptyUid,
  //                   emptyUid);

  // // flipped
  // map_->createTrack(SWITCH, 10, 0, 0, 10, 10, true, entrance2, exit2,
  //                   entrance2B, exit2B);

  // map_->createTrack(STATION, 0, 0, 10, 10, 0, true, entrance3, emptyUid,
  //                   emptyUid, emptyUid);

  // // // Connect tracks after they have all been created.
  // // // JsonDocument doc;
  // // // map_->mapToJson(doc);
  // // // serializeJsonPretty(doc, Serial);
  // // // Serial.println("============= calc ==========");
  // auto connect = [&](const Uid& a, const Uid& b) {
  //   Tag* tagA = map_->findTagByUid(a);
  //   Tag* tagB = map_->findTagByUid(b);
  //   const bool success = map_->connectTags(tagA, tagB);
  //   if (!success) {
  //     Serial.println("failed to connect");
  //   }
  // };

  // // // Create a continuous chain.
  // // if (success2 && success) {
  // //   connect(track.getTags()[0].getUid(), track2.getTags()[0].getUid());
  // // }
  // connect(exit0, entrance1);
  // connect(exit1, entrance2);
  // connect(exit2, entrance3);

  // // Position the map.
  // map_->calculateMap();

  // TrackPathFinder pathFinder(*map_);

  // std::vector<Uid> path;

  // Uid startUid = entrance0;
  // Uid destinationUid = entrance3;

  // if (pathFinder.findPath(startUid, destinationUid, path)) {
  //   Serial.println("Path found:");

  //   for (const Uid& uid : path) {
  //     Serial.println(uid.toString());
  //   }

  // } else {
  //   Serial.println("No path exists.");
  // }
  // JsonDocument doc1;
  // map_->mapToJson(doc1);
  // serializeJsonPretty(doc1, Serial);

  refresh();
}

void LvTrackMap::togglePathFind() {
  if (app.getScannerMode() == PATH_FINDING) {
    app.setScannerMode(app.getPreviousScannerMode());
  } else {
    app.setScannerMode(PATH_FINDING);
  }
  refreshAndSend();
}

void LvTrackMap::reset() {
  map_ = nullptr;

  transform_ = {};

  refresh();
}

void LvTrackMap::refreshAndSend() {
  refresh();
  if (sendStateToReceiverCallback != nullptr) {
    sendStateToReceiverCallback();
  }
}
// ============================================================
// REFRESH
// ============================================================
void LvTrackMap::updateAutoLabel() {
  display_.setLabelText(auto_label_, app.getPathFinderStatusText().c_str());
}

void LvTrackMap::refresh() {
  if (!root_ || lv_obj_is_hidden(root_)) {
    return;
  }
  display_.setButtonState(auto_btn_,
                          app.getScannerMode() == ScannerMode::PATH_FINDING);
  updateAutoLabel();
  if (map_->getMapModified()) {
    transformDirty_ = true;
    hitboxUid.clear();
    lv_obj_clean(hitBoxLayer_);
    // Serial.println("cleared");
    map_->setMapModified(false);
  }

  if (root_) {
    lv_obj_invalidate(root_);
  }
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
  // Serial.println("calculateTransform");
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
  const std::vector<Track>* tracks = map_->getTracks();

  for (const Track& track : *tracks) {
    if (!track.getPositioned()) {
      continue;
    }

    const auto& tags = track.getTags();

    // Include all calculated track points.
    for (const Tag& tag : tags) {
      includePoint(tag.getPoint().getX(), tag.getPoint().getY());
    }

    // Include the actual geometry of curved tracks.
    if (track.getTrackType() != CURVED || tags.empty()) {
      continue;
    }

    const float radius = track.getRadius();
    const float length = track.getLength();

    if (radius <= 0.0f || length <= 0.0f) {
      continue;
    }

    // if (tags.size() < 1) return;
    // TODO replace with logic that was used in other calc for straight
    const Point& entrance = tags[0].getPoint();

    const float direction = track.getDirection() ? -1.0f : 1.0f;

    const float heading = entrance.getHeading() * DEG_TO_RAD_F;

    const float centerHeading = heading + direction * (M_PI * 0.5f);

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

  const size_t trackCount = self->map_ ? self->map_->getTrackCount() : 0;

  const bool resized = width != self->lastWidth_ || height != self->lastHeight_;

  const bool tracksModified = trackCount != self->lastTrackCount_;

  if (self->transformDirty_ || resized || tracksModified) {
    self->calculateTransform(width, height);

    self->lastWidth_ = width;
    self->lastHeight_ = height;
    self->lastTrackCount_ = trackCount;

    self->transformDirty_ = false;
  }

  self->drawBackground(layer, area);
  // self->drawConnections(layer);

  if (self->map_) {
    const auto* tracks = self->map_->getTracks();

    size_t index = 0;

    for (const Track& track : *tracks) {
      self->drawTrack(layer, track, index++);
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

  // constexpr int32_t spacing = 20;

  // const int32_t width = lv_area_get_width(&area);
  // const int32_t height = lv_area_get_height(&area);

  // for (int32_t x = 0; x < width; x += spacing) {
  //   drawSegment(layer, {static_cast<float>(x), 0.0f, 0.0f},
  //               {static_cast<float>(x), static_cast<float>(height), 0.0f},
  //               config_.grid, 1);
  // }

  // for (int32_t y = 0; y < height; y += spacing) {
  //   drawSegment(layer, {0.0f, static_cast<float>(y), 0.0f},
  //               {static_cast<float>(width), static_cast<float>(y), 0.0f},
  //               config_.grid, 1);
  // }
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
                            lv_color_t color, bool filled,
                            float heading) const {
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

  const float cx = rootArea.x1 + center.getX();
  const float cy = rootArea.y1 + center.getY();

  lv_area_t area;
  area.x1 = static_cast<lv_coord_t>(lroundf(cx - radius));
  area.y1 = static_cast<lv_coord_t>(lroundf(cy - radius));
  area.x2 = static_cast<lv_coord_t>(lroundf(cx + radius));
  area.y2 = static_cast<lv_coord_t>(lroundf(cy + radius));

  // Draw circle.
  lv_draw_rect(layer, &dsc, &area);

  // Convert heading to radians.
  constexpr float DEG_TO_RAD_F = 0.017453292519943295f;
  const float angle = heading * DEG_TO_RAD_F;

  constexpr float ARROW_LENGTH = 12.0f;
  constexpr float ARROW_HEAD_LENGTH = 5.0f;
  constexpr float ARROW_HEAD_ANGLE = 0.6f;

  const float dx = cosf(angle);
  const float dy = sinf(angle);

  const float startX = cx + dx * (radius - 1);
  const float startY = cy + dy * (radius - 1);

  const float endX = cx + dx * (radius + ARROW_LENGTH);
  const float endY = cy + dy * (radius + ARROW_LENGTH);

  // Arrowhead coordinates.
  const float leftAngle = angle + M_PI - ARROW_HEAD_ANGLE;
  const float rightAngle = angle + M_PI + ARROW_HEAD_ANGLE;

  const float leftX = endX + cosf(leftAngle) * ARROW_HEAD_LENGTH;
  const float leftY = endY + sinf(leftAngle) * ARROW_HEAD_LENGTH;

  const float rightX = endX + cosf(rightAngle) * ARROW_HEAD_LENGTH;
  const float rightY = endY + sinf(rightAngle) * ARROW_HEAD_LENGTH;

  // Draw shaft.
  lv_draw_line_dsc_t lineDsc;
  lv_draw_line_dsc_init(&lineDsc);

  lineDsc.color = color;
  lineDsc.width = 2;
  lineDsc.opa = LV_OPA_COVER;

  lineDsc.p1.x = static_cast<lv_value_precise_t>(lroundf(startX));
  lineDsc.p1.y = static_cast<lv_value_precise_t>(lroundf(startY));
  lineDsc.p2.x = static_cast<lv_value_precise_t>(lroundf(endX));
  lineDsc.p2.y = static_cast<lv_value_precise_t>(lroundf(endY));

  lv_draw_line(layer, &lineDsc);

  // Draw left arrowhead.
  lineDsc.p1 = lineDsc.p2;
  lineDsc.p2.x = static_cast<lv_value_precise_t>(lroundf(leftX));
  lineDsc.p2.y = static_cast<lv_value_precise_t>(lroundf(leftY));

  lv_draw_line(layer, &lineDsc);

  // Draw right arrowhead.
  lineDsc.p1.x = static_cast<lv_value_precise_t>(lroundf(endX));
  lineDsc.p1.y = static_cast<lv_value_precise_t>(lroundf(endY));
  lineDsc.p2.x = static_cast<lv_value_precise_t>(lroundf(rightX));
  lineDsc.p2.y = static_cast<lv_value_precise_t>(lroundf(rightY));

  lv_draw_line(layer, &lineDsc);
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

  const std::vector<Track>* tracks = map_->getTracks();
  for (const Track& track : *tracks) {
    if (!track.getPositioned()) {
      continue;
    }

    for (const Tag& tag : track.getTags()) {
      // No connection stored.
      if (!tag.getConnectedTagUid().isValid()) {
        continue;
      }

      const Tag* otherTag = map_->findTagByUid(tag.getConnectedTagUid());

      if (!otherTag) {
        continue;
      }

      // Connection exists in both directions.
      // Only draw it once.
      if (std::less<const Tag*>{}(otherTag, &tag)) {
        continue;
      }

      const Point a = worldToMap(tag.getPoint());
      const Point b = worldToMap(otherTag->getPoint());

      drawSegment(layer, a, b, config_.connection, config_.connectionWidth);
    }
  }
}

// ============================================================
// TRACK DRAWING
// ============================================================

void LvTrackMap::drawTrack(lv_layer_t* layer, const Track& track,
                           size_t index) {
  if (!track.getPositioned()) {
    // Serial.println("track has not been positioned");

    return;
  }

  switch (track.getTrackType()) {
    case BUMPER:
      drawBumper(layer, track);
    case STRAIGHT:
    case CURVED:
      // if (track.getTags().size() < 2) return;
      drawCurveOrStraight(layer, worldToMap(track.getTags()[0].getPoint()),
                          worldToMap(track.getTags()[1].getPoint()));

      break;

    case SWITCH:
      // if (track.getTags().size() < 4) return;

      drawCurveOrStraight(layer, worldToMap(track.getTags()[0].getPoint()),
                          worldToMap(track.getTags()[1].getPoint()));
      drawCurveOrStraight(layer, worldToMap(track.getTags()[2].getPoint()),
                          worldToMap(track.getTags()[3].getPoint()));
      break;

    case CROSS:
      // if (track.getTags().size() < 4) return;

      drawCurveOrStraight(layer, worldToMap(track.getTags()[0].getPoint()),
                          worldToMap(track.getTags()[1].getPoint()));
      drawCurveOrStraight(layer, worldToMap(track.getTags()[2].getPoint()),
                          worldToMap(track.getTags()[3].getPoint()));
      break;
    case STATION:
      drawCurveOrStraight(layer, worldToMap(track.getTags()[0].getPoint()),
                          worldToMap(track.getTags()[0].getPoint()));
      const Point point = worldToMap(track.getTags()[0].getPoint());

      drawCircle(layer, point, config_.tagRadius, config_.tagConnected, true,
                 point.getHeading());
      const Uid destinationUid = track.getTags()[0].getUid();
      createTagHitBox(destinationUid, point);
      drawLabel(layer, track, index);

      break;
  }

  // drawTags(layer, track.getTags());
  // drawLabel(layer, track, index);
}
void LvTrackMap::createTagHitBox(const Uid destinationUid, const Point point) {
  // need to clear out when ui gets refreshed
  if (!destinationUid.isValid()) {
    return;
  }

  auto it = std::find_if(
      hitboxUid.begin(), hitboxUid.end(),
      [destinationUid](Uid& p) { return p.equals(destinationUid); });

  if (it != hitboxUid.end()) {
    return;
  }
  // Serial.println("createTagHitBox");

  hitboxUid.emplace_back(destinationUid);
  display_.createTagHitbox(hitBoxLayer_, point.getX(), point.getY(),
                           config_.tagRadius * 4, [this, destinationUid]() {
                             Serial.println("touch hitbox");
                             app.setDestinationUid(destinationUid);
                             refreshAndSend();
                           });
}
void LvTrackMap::drawCurveOrStraight(lv_layer_t* layer, const Point entrance,
                                     const Point exit) const {
  constexpr int MIN_SEGMENTS = 4;
  constexpr int MAX_SEGMENTS = 32;
  constexpr float MAX_SEGMENT_LENGTH = 30.0f;

  const float x1 = entrance.getX();
  const float y1 = entrance.getY();
  const float x2 = exit.getX();
  const float y2 = exit.getY();

  // Calculate endpoint tangent directions.
  const float h1 = entrance.getHeading() * DEG_TO_RAD_F;
  const float h2 = exit.getHeading() * DEG_TO_RAD_F;

  // Calculate perpendicular normals.
  const float n1x = -sinf(h1);
  const float n1y = cosf(h1);

  const float n2x = -sinf(h2);
  const float n2y = cosf(h2);

  // Find intersection of the normal lines.
  const float denominator = n1x * n2y - n1y * n2x;

  // Parallel normals indicate a straight track.
  if (fabsf(denominator) < 0.0001f) {
    drawSegment(layer, (entrance), (exit), lv_color_hex(0x00FF00), 4);
    return;
  }

  const float dx = x2 - x1;
  const float dy = y2 - y1;

  const float t = (dx * n2y - dy * n2x) / denominator;

  const float cx = x1 + t * n1x;
  const float cy = y1 + t * n1y;

  const float radius = hypotf(x1 - cx, y1 - cy);

  if (radius < 0.001f) {
    drawSegment(layer, (entrance), (exit), lv_color_hex(0x00FF00), 4);
    return;
  }

  // Calculate arc angles.
  const float startAngle = atan2f(y1 - cy, x1 - cx);
  const float endAngle = atan2f(y2 - cy, x2 - cx);

  float sweep = endAngle - startAngle;

  while (sweep > M_PI) sweep -= 2.0f * M_PI;
  while (sweep < -M_PI) sweep += 2.0f * M_PI;

  // Determine number of segments based on arc length.
  const float arcLength = fabsf(sweep) * radius;

  const int segments = std::min(
      MAX_SEGMENTS,
      std::max(MIN_SEGMENTS,
               static_cast<int>(ceilf(arcLength / MAX_SEGMENT_LENGTH))));

  Point previous = (entrance);

  for (int i = 1; i <= segments; ++i) {
    const float angle = startAngle + sweep * (static_cast<float>(i) / segments);

    Point worldPoint(cx + radius * cosf(angle), cy + radius * sinf(angle),
                     0.0f);

    Point current = (worldPoint);

    drawSegment(layer, previous, current, lv_color_hex(0x00FF00), 4);

    previous = current;
  }
}

// ============================================================
// BUMPER
// ============================================================

void LvTrackMap::drawBumper(lv_layer_t* layer, const Track& track) const {
  const Point endpoint = worldToMap(track.getTags()[1].getPoint());

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
  // static constexpr const char* labels[] = {"entranceA", "entranceB", "exitA",
  //                                          "exitB"};

  const auto& tags = track.getTags();

  for (size_t i = 0; i < tags.size(); ++i) {
    const Tag& tag = tags[i];

    if (!tag.getUid().isValid()) {
      continue;
    }

    const Point point = worldToMap(tag.getPoint());

    const bool connected =
        map_->findTagByUid(tag.getConnectedTagUid()) != nullptr;

    drawCircle(layer, point, config_.tagRadius,
               connected ? config_.tagConnected : config_.tagUnconnected, true,
               point.getHeading());

    // if (i < sizeof(labels) / sizeof(labels[0])) {
    //   drawText(layer, {point.getX(), point.getY() - 10.0f, 0.0f}, labels[i],
    //            lv_color_hex(0x111111), true);
    // }
  }
}

// ============================================================
// TRACK LABEL
// ============================================================

void LvTrackMap::drawLabel(lv_layer_t* layer, const Track& track,
                           size_t index) const {
  if (track.getTags().size() < 2) return;

  const Point a = worldToMap(track.getTags()[0].getPoint());

  const Point b = worldToMap(track.getTags()[1].getPoint());
  // todo check angle
  Point center = {(a.getX() + b.getX()) * 0.5f,
                  (a.getY() + b.getY()) * 0.5f - 12.0f, 0.0f};

  char text[64];

  snprintf(text, sizeof(text), "#%u %s", static_cast<unsigned>(index),
           track.trackTypeToString());

  drawText(layer, center, text, config_.label, true);
}

void LvTrackMap::toggle() {
  // Serial.print("call toggle");

  if (lv_obj_is_hidden(root_)) {
    // lv_obj_move_to_index(root_, 0);

    lv_obj_set_hidden(root_, false);
    lv_obj_move_foreground(root_);
    // Serial.print("show");
  } else {
    lv_obj_set_hidden(root_, true);
    // lv_obj_move_to_index(root_, 2);

    // Serial.print("hide");
  }
}