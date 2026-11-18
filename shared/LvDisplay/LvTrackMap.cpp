#include "LvTrackMap.h"

namespace {

constexpr float DEG_TO_RAD_F = M_PI / 180.0f;

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

  lv_obj_set_style_bg_color(root_, lv_color_hex(colorConfig.backgroundColor),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
  // / lv_obj_set_style_bg_color(
  //     grid_cont, lv_color_hex(colorConfig.backgroundColor), LV_PART_MAIN);
  lv_obj_set_scrollable(root_, false);

  installCallbacks();

  lv_obj_t* grid_cont = lv_obj_create(root_);
  lv_obj_set_width(grid_cont, lv_pct(100));
  lv_obj_set_height(grid_cont, lv_pct(100));

  /* Center the container on the screen */
  lv_obj_center(grid_cont);

  lv_obj_set_style_bg_opa(grid_cont, LV_OPA_TRANSP, LV_PART_MAIN);

  static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_TEMPLATE_LAST};

  /* Define 2 rows: equal height */
  static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_FR(7), LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(grid_cont, col_dsc, row_dsc);
  lv_obj_set_layout(grid_cont, LV_LAYOUT_GRID);

  count_label_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.statusColor), 8, 53, LV_PART_MAIN,
      LV_ALIGN_TOP_LEFT, [this]() -> const char* {
        static char text[32];
        snprintf(text, sizeof(text), "Tracks: %u",
                 static_cast<unsigned>(app.getCurrentTracks()->size()));
        return text;
      });
  lv_obj_set_grid_cell(count_label_, LV_GRID_ALIGN_CENTER, 1, 1,
                       LV_GRID_ALIGN_CENTER, 0, 1);
  hide_button_ = sharedDisplay.createActionButton(
      grid_cont, "Hide", [this](lv_event_t* event) { toggle(); },
      lv_color_hex(colorConfig.menuHeaderColor));
  lv_obj_set_grid_cell(hide_button_, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);
  print_button_ = sharedDisplay.createActionButton(
      grid_cont, "Print",
      [this](lv_event_t* event) {
        JsonDocument outputDoc;
        getJsonFromState(outputDoc);
        String outputString;
        serializeJson(outputDoc, Serial);
      },
      lv_color_hex(colorConfig.menuHeaderColor));
  lv_obj_set_grid_cell(print_button_, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);
  auto_btn_ = sharedDisplay.createActionButton(
      grid_cont, "Auto", [this](lv_event_t* event) { togglePathFind(); },
      lv_color_hex(colorConfig.menuHeaderColor),
      [this]() { return app.getScannerMode() == PATH_FINDING; });
  lv_obj_set_grid_cell(auto_btn_, LV_GRID_ALIGN_STRETCH, 2, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);
  auto_label_ = sharedDisplay.createLabel(
      grid_cont, lv_color_hex(colorConfig.statusColor), 8, 53, LV_PART_MAIN,
      LV_ALIGN_TOP_LEFT, "Auto status");
  lv_obj_set_grid_cell(auto_label_, LV_GRID_ALIGN_CENTER, 2, 1,
                       LV_GRID_ALIGN_CENTER, 2, 1);

  hitBoxLayer_ = lv_obj_create(grid_cont);

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

  lv_obj_move_foreground(print_button_);
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

void LvTrackMap::togglePathFind() {
  if (app.getScannerMode() == PATH_FINDING &&
      app.getPreviousScannerMode() != PATH_FINDING) {
    app.setScannerMode(app.getPreviousScannerMode());
  } else if (app.getScannerMode() == PATH_FINDING) {
    app.setScannerMode(SCANNING_TRACKS);
  } else {
    app.setScannerMode(PATH_FINDING);
  }
  refreshAndSend();
}

void LvTrackMap::reset() {
  // map_ = nullptr;

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
  sharedDisplay.setLabelText(auto_label_,
                             app.getPathFinderStatusText().c_str());
}

Tracks pastTracks;
bool hasPastTracks = false;

void LvTrackMap::refresh() {
  if (!root_ || lv_obj_is_hidden(root_)) {
    return;
  }
  // sharedDisplay.setButtonState(auto_btn_,
  //                         app.getScannerMode() == ScannerMode::PATH_FINDING);
  // updateAutoLabel();
  Tracks* currentTracks = app.getCurrentTracks();

  if (!currentTracks) return;

  if (!hasPastTracks || pastTracks != *currentTracks) {
    transformDirty_ = true;
    hitboxUid.clear();
    lv_obj_clean(hitBoxLayer_);

    pastTracks = *currentTracks;
    hasPastTracks = true;
  }
  sharedDisplay.updateStates();

  if (root_) {
    lv_obj_invalidate(root_);
  }
}

// ============================================================
// VIEW / TRANSFORM
// ============================================================
void LvTrackMap::calculateTransform(lv_coord_t width, lv_coord_t height) {
  transform_ = {};
  transform_.scale = 1.0f;

  if (!root_) {
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

  const Tracks* tracks = app.getCurrentTracks();

  if (!tracks) return;

  for (const Track& track : *tracks) {
    if (!track.getPositioned()) continue;
    if (track.getTrackType() == CURVED) {
      std::vector<Point> points;

      calculatePointsForCurvedOrStraight(track.getTags()[0].getPoint(),
                                         track.getTags()[1].getPoint(), points);

      for (int i = 0; i < points.size(); ++i) {
        includePoint(points[i].getX(), points[i].getY());
      }
    } else if (track.getTrackType() == SWITCH) {
      std::vector<Point> points;

      calculatePointsForCurvedOrStraight(track.getTags()[0].getPoint(),
                                         track.getTags()[1].getPoint(), points);
      calculatePointsForCurvedOrStraight(track.getTags()[2].getPoint(),
                                         track.getTags()[3].getPoint(), points);
      for (int i = 0; i < points.size(); ++i) {
        includePoint(points[i].getX(), points[i].getY());
      }
    }
    for (const Tag& tag : track.getTags()) {
      const Point& p = tag.getPoint();

      includePoint(p.getX(), p.getY());
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

  const size_t trackCount = app.getCurrentTracks()->size();

  const bool resized = width != self->lastWidth_ || height != self->lastHeight_;

  const bool tracksModified = trackCount != self->lastTrackCount_;
  self->calculateTransform(width, height);

  if (self->transformDirty_ || resized || tracksModified) {
    Serial.println("called calculate transform");
    self->calculateTransform(width, height);

    self->lastWidth_ = width;
    self->lastHeight_ = height;
    self->lastTrackCount_ = trackCount;

    self->transformDirty_ = false;
  }

  const Tracks* tracks = app.getCurrentTracks();

  size_t index = 0;

  for (const Track& track : *tracks) {
    self->drawTrack(layer, track, index++);
  }
}
// ============================================================
// TRACK DRAWING
// ============================================================

void LvTrackMap::drawTrack(lv_layer_t* layer, const Track& track,
                           size_t index) {
  if (!track.getPositioned()) {
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

  hitboxUid.emplace_back(destinationUid);
  sharedDisplay.createTagHitbox(hitBoxLayer_, point.getX(), point.getY(),
                                config_.tagRadius * 4,
                                [this, destinationUid](lv_event_t* event) {
                                  Serial.println("touch hitbox");
                                  app.setDestinationUid(destinationUid);
                                  refreshAndSend();
                                });
}
void LvTrackMap::calculatePointsForCurvedOrStraight(
    const Point entrance, const Point exit, std::vector<Point>& points) const {
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
    points.emplace_back(entrance);
    points.emplace_back(exit);
    return;
  }

  const float dx = x2 - x1;
  const float dy = y2 - y1;

  const float t = (dx * n2y - dy * n2x) / denominator;

  const float cx = x1 + t * n1x;
  const float cy = y1 + t * n1y;

  const float radius = hypotf(x1 - cx, y1 - cy);

  if (radius < 0.001f) {
    points.emplace_back(entrance);
    points.emplace_back(exit);
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
    points.emplace_back(previous);
    points.emplace_back(current);
    previous = current;
  }
}

void LvTrackMap::drawCurveOrStraight(lv_layer_t* layer, const Point entrance,
                                     const Point exit) const {
  std::vector<Point> points;

  calculatePointsForCurvedOrStraight(entrance, exit, points);

  for (int i = 0; i < points.size(); i+=2) {
    drawSegment(layer, points[i], points[i+1], lv_color_hex(0x00FF00), 4);
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

  const Tags& tags = track.getTags();

  for (size_t i = 0; i < tags.size(); ++i) {
    const Tag& tag = tags[i];

    if (!tag.getUid().isValid()) {
      continue;
    }

    const Point point = worldToMap(tag.getPoint());

    const bool connected = app.getCurrentTracks()->findTagByUid(
                               tag.getConnectedTagUid()) != nullptr;

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
  //  TODO maybe use shared display calls?
  drawText(layer, center, text, lv_color_hex(colorConfig.textColor), true);
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