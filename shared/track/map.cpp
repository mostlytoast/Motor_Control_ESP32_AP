#include "map.h"

// ============================================================
// MATH HELPERS
// ============================================================

static float degreesToRadians(float degrees) { return degrees * M_PI / 180.0f; }

static float radiansToDegrees(float radians) { return radians * 180.0f / M_PI; }

static float normalizeHeading(float heading) {
  while (heading >= 360.0f) heading -= 360.0f;
  while (heading < 0.0f) heading += 360.0f;

  return heading;
}

static Point movePoint(Point start, float heading, float distance) {
  float radians = degreesToRadians(heading);
  // todo have to check heading

  return Point(start.getX() + cos(radians) * distance,
               start.getY() + sin(radians) * distance, heading);
}

static Point rotatePoint(Point point, float heading) {
  float radians = degreesToRadians(heading);
  float c = cos(radians);
  float s = sin(radians);
  // todo have to check heading
  return Point(point.getX() * c - point.getY() * s,
               point.getX() * s + point.getY() * c, heading);
}

// ============================================================
// TAG HELPERS
// ============================================================

static bool isEntrance(Track& track, Tag* tag) {
  return tag == &track.getTags()[0] || tag == &track.getTags()[1];
}

static bool isExit(Track& track, Tag* tag) {
  return tag == &track.getTags()[2] || tag == &track.getTags()[3];
}

// ============================================================
// STRAIGHT TRACK
// ============================================================

void TrackMap::calculateStraightPose(Track& track) {
  track.getPoints()[0].setHeading(
      normalizeHeading(track.getPoints()[0].getHeading()));

  track.getPoints()[2] =
      movePoint(track.getPoints()[0], track.getPoints()[0].getHeading(),
                track.getLength());

  track.getPoints()[2].setHeading(track.getPoints()[0].getHeading());

  track.setPositioned(true);
}

void TrackMap::calculateStraightFromExit(Track& track) {
  track.getPoints()[2].setHeading(
      normalizeHeading(track.getPoints()[2].getHeading()));

  track.getPoints()[0] =
      movePoint(track.getPoints()[2], track.getPoints()[2].getHeading(),
                -track.getLength());

  track.getPoints()[0].setHeading(track.getPoints()[2].getHeading());

  track.setPositioned(true);
}

// ============================================================
// CALCULATE FROM ENTRANCE
// ============================================================

void TrackMap::calculateFromEntrance(Track& track) {
  Serial.print("calculateFromEntrance serial type ");
  Serial.println(track.getTrackType());

  switch (track.getTrackType()) {
    case STRAIGHT:
    // uses same as bumper
    case BUMPER:
      calculateStraightPose(track);
      break;

    case CURVED:
      calculateCurvePose(track);
      break;

    case SWITCH:
      calculateSwitchPose(track);
      break;

    case CROSS:
      calculateCrossPose(track);
      return;
    default:
      Serial.println("wrong type ");
  }
}

// ============================================================
// CALCULATE FROM EXIT
// ============================================================

void TrackMap::calculateFromExit(Track& track) {
  Serial.print("calculateFromExit serial type ");
  Serial.println(track.getTrackType());
  switch (track.getTrackType()) {
    case STRAIGHT:
    case BUMPER:
    case SWITCH:
    case CROSS:
      calculateStraightFromExit(track);
      break;

    case CURVED:
      calculateCurveFromExit(track);
      return;
    default:
      Serial.println("wrong type ");
  }
}

// ============================================================
// CURVE FROM ENTRANCE
// ============================================================

void TrackMap::calculateCurvePose(Track& track) {
  Serial.println("calculateCurvePose");

  const float radius = track.getRadius();

  if (radius <= 0.0f) {
    calculateStraightPose(track);
    return;
  }

  const float length = track.getLength();

  const float angleRadians = length / radius;
  const float angleDegrees = radiansToDegrees(angleRadians);

  const float heading = normalizeHeading(track.getPoints()[0].getHeading());

  const float turn = track.getDirection() ? -angleDegrees : angleDegrees;

  const float centerHeading =
      track.getDirection() ? heading - 90.0f : heading + 90.0f;

  const Point center = movePoint(track.getPoints()[0], centerHeading, radius);

  const float startAngle = atan2(track.getPoints()[0].getY() - center.getY(),
                                 track.getPoints()[0].getX() - center.getX());

  const float endAngle = startAngle + degreesToRadians(turn);

  const float exitX = center.getX() + cos(endAngle) * radius;
  const float exitY = center.getY() + sin(endAngle) * radius;

  const float exitHeading = normalizeHeading(heading + turn);

  // Primary curve exit is point 2, matching straight tracks.
  track.getPoints()[2] = Point(exitX, exitY, exitHeading);

  track.setPositioned(true);

  Serial.printf("Curve entrance: %.2f, %.2f, heading %.2f\n",
                track.getPoints()[0].getX(), track.getPoints()[0].getY(),
                track.getPoints()[0].getHeading());

  Serial.printf("Curve exit: %.2f, %.2f, heading %.2f\n",
                track.getPoints()[2].getX(), track.getPoints()[2].getY(),
                track.getPoints()[2].getHeading());
}

// ============================================================
// CURVE FROM EXIT
// ============================================================

void TrackMap::calculateCurveFromExit(Track& track) {
  float radius = track.getRadius();

  if (radius <= 0.0f) {
    calculateStraightFromExit(track);
    return;
  }

  float angleRadians = track.getLength() / radius;
  float angleDegrees = radiansToDegrees(angleRadians);

  float turn = track.getDirection() ? -angleDegrees : angleDegrees;

  float exitHeading = normalizeHeading(track.getPoints()[2].getHeading());

  float entranceHeading = normalizeHeading(exitHeading - turn);

  float centerHeading =
      track.getDirection() ? exitHeading - 90.0f : exitHeading + 90.0f;

  Point center = movePoint(track.getPoints()[2], centerHeading, radius);

  float exitAngle = atan2(track.getPoints()[2].getY() - center.getY(),
                          track.getPoints()[2].getX() - center.getX());

  float entranceAngle = exitAngle - degreesToRadians(turn);

  track.getPoints()[0] =
      Point(center.getX() + cos(entranceAngle) * radius,
            center.getY() + sin(entranceAngle) * radius, entranceAngle);

  track.getPoints()[0].setHeading(entranceHeading);

  track.setPositioned(true);
}

// ============================================================
// SWITCH POSE
// ============================================================

void TrackMap::calculateSwitchPose(Track& track) {
  float heading = normalizeHeading(track.getPoints()[0].getHeading());

  // Main route
  track.getPoints()[1] =
      movePoint(track.getPoints()[0], heading, track.getLength());

  track.getPoints()[2].setHeading(heading);

  // Curved branch
  float angle = degreesToRadians(track.getBranchAngle());

  float radius = track.getBranchRadius();

  // TODO check angle
  Point local = Point(radius * sin(angle), radius * (1.0f - cos(angle)), angle);

  Point offset = rotatePoint(local, heading);

  // todo check heading
  track.getPoints()[3] =
      Point(track.getPoints()[0].getX() + offset.getX(),
            track.getPoints()[0].getY() + offset.getY(), heading);

  track.getPoints()[3].setHeading(
      normalizeHeading(heading + track.getBranchAngle() + 10.0f));

  track.setPositioned(true);
}

// ============================================================
// CROSS POSE
// ============================================================

void TrackMap::calculateCrossPose(Track& track) {
  calculateStraightPose(track);
}

// ============================================================
// POSITION CONNECTED TRACK
// ============================================================

bool TrackMap::positionConnectedTrack(Track& source, Tag& sourceTag) {
  Serial.println("positionConnectedTrack");

  Tag* destinationTag = findTagByUid(sourceTag.getConnectedTagUid());
  if (!destinationTag) return false;
  Track* destinationTrack = findTrackByUid(destinationTag->getUid());
  if (!destinationTrack) return false;
  Serial.println("positionConnectedTrack1");

  if (!destinationTrack) return false;
  Serial.println("positionConnectedTrack2");

  if (destinationTrack->getPositioned()) return true;

  bool sourceIsEntrance = isEntrance(source, &sourceTag);

  bool sourceIsExit = isExit(source, &sourceTag);
  Serial.println("positionConnectedTrack3");

  if (!sourceIsEntrance && !sourceIsExit) return false;

  Point sourcePoint;
  float sourceHeading;

  if (sourceIsEntrance) {
    Serial.println("positionConnectedTrack4");

    sourcePoint = source.getPoints()[0];

    sourceHeading = source.getPoints()[0].getHeading();

  } else {
    Serial.println("positionConnectedTrack5");

    sourcePoint = source.getPoints()[2];
    sourceHeading = source.getPoints()[2].getHeading();
  }

  sourceHeading = normalizeHeading(sourceHeading);

  bool destinationIsEntrance = isEntrance(*destinationTrack, destinationTag);

  bool destinationIsExit = isExit(*destinationTrack, destinationTag);

  if (!destinationIsEntrance && !destinationIsExit) {
    Serial.println("positionConnectedTrack6");

    return false;
  }

  float headingAwayFromSource = sourceIsEntrance
                                    ? normalizeHeading(sourceHeading + 180.0f)
                                    : sourceHeading;
  Serial.println("positionConnectedTrack7");

  if (destinationIsEntrance) {
    Serial.println("positionConnectedTrack8");

    destinationTrack->getPoints()[0] = sourcePoint;

    destinationTrack->getPoints()[0].setHeading(headingAwayFromSource);

    calculateFromEntrance(*destinationTrack);

    return destinationTrack->getPositioned();
  }

  destinationTrack->getPoints()[2] = sourcePoint;

  destinationTrack->getPoints()[2].setHeading(
      normalizeHeading(headingAwayFromSource + 180.0f));

  calculateFromExit(*destinationTrack);
  Serial.print("positionConnectedTrack9 positioned");
  Serial.println(destinationTrack->getPositioned());

  return destinationTrack->getPositioned();
}

// ============================================================
// CALCULATE ENTIRE MAP
// ============================================================

void TrackMap::calculateMap() {
  // Serial.println("calculating map");
  if (tracks.size() == 0) return;

  for (uint8_t i = 0; i < tracks.size(); i++) {
    tracks[i].setPositioned(false);
  }

  // Track 0 is the world origin.
  tracks[0].getPoints()[0] = Point(0.0f, 0.0f, 0.0f);

  tracks[0].getPoints()[0].setHeading(0.0f);

  calculateFromEntrance(tracks[0]);
  Serial.print("track positioned ");
  Serial.println(tracks[0].getPositioned());
  bool changed;

  do {
    changed = false;

    for (uint8_t i = 0; i < tracks.size(); i++) {
      Track& track = tracks[i];

      if (track.getPositioned()) continue;

      Serial.print("calculating map ");
      Serial.print(track.getPositioned());
      Serial.print(" i ");

      Serial.println(i);

      for (Tag tag : track.getTags()) {
        if (positionConnectedTrack(track, tag)) changed = true;
      }
    }
  } while (changed);
}

// ============================================================
// CREATE TRACK
// ============================================================

Track* TrackMap::createTrack(TrackType type, float length, float radius,
                             float branchAngle, float branchRadius,
                             float branchLength, bool direction,
                             Uid entranceAUid, Uid exitAUid, Uid entranceBUid,
                             Uid exitBUid, bool dontAdd,
                             Uid connectEntranceAUid, Uid connectExitAUid,
                             Uid connectEntranceBUid, Uid connectExitBUid) {
  Track track = Track(type, length, radius, branchAngle, branchRadius,
                      branchLength, direction, entranceAUid, exitAUid,
                      entranceBUid, exitBUid, dontAdd, connectEntranceAUid,
                      connectExitAUid, connectEntranceBUid, connectExitBUid);
  if (dontAdd) {
    // Use temporary storage without changing tracks.size()
    tempTrack = track;

    return &tempTrack;

  } else if (tracks.size() >= MAX_TRACKS && !dontAdd) {
    return nullptr;
  } else {
    // Add to the tracks array
    tracks.emplace_back(track);
    Serial.println("added tracks");
    return &tracks.back();
  }
}

void TrackMap::addTrack(Track track) { tracks.emplace_back(track); }

// Station* TrackMap::createStation(Track* track, Uid stationUID, bool dontAdd)
// { Station* station; station->tag->uid = stationUID; station->tag->track =
// track; if (dontAdd) {
//   return station;
// } else {
//   track->station.tag->uid = stationUID;
// }
// }
// ============================================================
// CONNECT TAGS
// ============================================================

bool TrackMap::connectTags(Tag* a, Tag* b) {
  Track* trackA = findTrackByUid(a->getUid());
  Track* trackB = findTrackByUid(b->getUid());

  if (!a || !b || !trackA || !trackB) {
    Serial.println("one of the tags is null");
    return false;
  }

  // a->connectedTag = b;
  a->getConnectedTagUid() = b->getUid();
  // b->connectedTag = a;
  b->getConnectedTagUid() = a->getUid();

  if (trackA) {
    trackA->setPositioned(false);
  }

  if (trackB) {
    trackB->setPositioned(false);
  }
  Serial.println("Connected");

  return true;
}

// ============================================================
// GET TRACK
// ============================================================

Track* TrackMap::getTrack(uint8_t index) {
  if (index >= tracks.size()) return nullptr;

  return &tracks[index];
}

// ============================================================
// FIND TRACK
// ============================================================

Track* TrackMap::findTrackByUid(Uid uid) {
  for (uint8_t i = 0; i < tracks.size(); i++) {
    Track& trackSearch = tracks[i];

    for (Tag& tagSearch : trackSearch.getTags()) {
      if (tagSearch.getUid().equals(uid)) {
        return &trackSearch;
      }
    }
  }

  Serial.println("did not find tag");

  return nullptr;
}

// ============================================================
// FIND TAG BY UID
// ============================================================

Tag* TrackMap::findTagByUid(Uid uid) {
  for (uint8_t i = 0; i < tracks.size(); i++) {
    Track& trackSearch = tracks[i];

    for (Tag& tagSearch : trackSearch.getTags()) {
      if (tagSearch.getUid().equals(uid)) {
        return &tagSearch;
      }
    }
  }

  Serial.println("did not find tag");

  return nullptr;
}

// ============================================================
// DOES TRACK EXIST
// ============================================================
bool TrackMap::doUidsBelongToSameTrack(const Uid& uidA, const Uid& uidB) {
  Track* track = findTrackByUid(uidA);
  if (!track) {
    Serial.println("Tag was not found in the track list.");

    return false;
  }

  for (Tag tagSearch : track->getTags()) {
    if (tagSearch.getUid().equals(uidB)) {
      return true;
    }
  }
  return false;
}

bool TrackMap::doesTrackExist(Uid uid) { return findTagByUid(uid) != nullptr; }

// ============================================================
// GET TRACK COUNT
// ============================================================

uint8_t TrackMap::getTrackCount() const { return tracks.size(); }

int TrackMap::trackIndex(const Track* track) const {
  if (!track) {
    return -1;
  }
  // todo use vector operations
  const uint8_t count = getTrackCount();

  for (uint8_t i = 0; i < count; ++i) {
    const Track current = tracks.at(i);

    if (current == *track) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void TrackMap::mapToJson(JsonDocument& doc) {
  JsonArray tracksJson = doc["tracks"].to<JsonArray>();
  int count = 0;
  for (uint8_t i = 0; i < tracks.size(); i++) {
    JsonObject obj = tracksJson.add<JsonObject>();

    Track& track = tracks[i];

    if (track.getInclude()) {
      obj["id"] = count;

      track.trackToJson(obj);
      count++;
    }
  }
  // Serial.println("tracks json");
  // serializeJsonPretty(tracksJson,Serial);
}

void TrackMap::JsonToMap(JsonArray doc) {
  // todo find better way to prevent it creating multiple of same track
  // if (tracks.size() ==1 ) return;
  clearMap();
  if (doc.isNull()) {
    Serial.println("JsonToMap: no tracks array found");
    return;
  }

  for (uint8_t i = 0; i < doc.size(); i++) {
    if (tracks.size() >= MAX_TRACKS) {
      continue;
    } else {
      // Add to the tracks array
      bool success;
      Track track = Track(doc[i].as<JsonObject>(), success);
      if (success) tracks.emplace_back(track);
    }
  }
  trackMap.calculateMap();
}

void TrackMap::clearMap() { tracks.clear(); }
