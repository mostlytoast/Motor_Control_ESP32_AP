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
  return tag == &track.getTags()[0] || tag == &track.getTags()[2];
}

static bool isExit(Track& track, Tag* tag) {
  return tag == &track.getTags()[1] || tag == &track.getTags()[3];
}

// ============================================================
// STRAIGHT TRACK
// ============================================================

void TrackMap::calculateStraightPose(Track& track) {
  Point& entrance = track.getPoints()[0];
  Point& exit = track.getPoints()[1];

  entrance.setHeading(normalizeHeading(entrance.getHeading()));

  exit = movePoint(entrance, entrance.getHeading(), track.getLength());

  exit.setHeading(entrance.getHeading());

  track.setPositioned(true);
}

void TrackMap::calculateStraightFromExit(Track& track) {
  Point& entrance = track.getPoints()[0];
  Point& exit = track.getPoints()[1];

  exit.setHeading(normalizeHeading(exit.getHeading()));

  entrance = movePoint(exit, exit.getHeading(), -track.getLength());

  entrance.setHeading(exit.getHeading());

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
  const float radius = track.getRadius();

  if (radius <= 0.0f) {
    calculateStraightPose(track);
    return;
  }

  const float length = track.getLength();

  if (length <= 0.0f) {
    return;
  }

  Point& entrance = track.getPoints()[0];
  Point& exit = track.getPoints()[1];

  const float heading = normalizeHeading(entrance.getHeading());

  // Angular displacement in degrees.
  const float turn = track.getDirection() ? -radiansToDegrees(length / radius)
                                          : radiansToDegrees(length / radius);

  // Find the center of the circle.
  const float centerHeading = heading + (track.getDirection() ? -90.0f : 90.0f);

  const Point center = movePoint(entrance, centerHeading, radius);

  // Calculate the entrance angle relative to the center.
  const float startAngle =
      atan2(entrance.getY() - center.getY(), entrance.getX() - center.getX());

  // Rotate around the circle.
  const float endAngle = startAngle + degreesToRadians(turn);

  // Calculate exit coordinates.
  exit.setX(center.getX() + cos(endAngle) * radius);

  exit.setY(center.getY() + sin(endAngle) * radius);

  exit.setHeading(normalizeHeading(heading + turn));

  track.setPositioned(true);
  Serial.printf("CURVE RESULT: P0=(%.2f, %.2f, %.2f) P1=(%.2f, %.2f, %.2f)\n",
                entrance.getX(), entrance.getY(), entrance.getHeading(),
                exit.getX(), exit.getY(), exit.getHeading());
}
// ============================================================
// CURVE FROM EXIT
// ============================================================

void TrackMap::calculateCurveFromExit(Track& track) {
  const float radius = track.getRadius();

  if (radius <= 0.0f) {
    calculateStraightFromExit(track);
    return;
  }

  const float length = track.getLength();

  if (length <= 0.0f) {
    return;
  }

  Point& entrance = track.getPoints()[0];
  Point& exit = track.getPoints()[1];

  const float angleDegrees = radiansToDegrees(length / radius);

  const float turn = track.getDirection() ? -angleDegrees : angleDegrees;

  const float exitHeading = normalizeHeading(exit.getHeading());

  const float entranceHeading = normalizeHeading(exitHeading - turn);

  const float centerHeading =
      exitHeading + (track.getDirection() ? -90.0f : 90.0f);

  const Point center = movePoint(exit, centerHeading, radius);

  const float exitAngle =
      atan2(exit.getY() - center.getY(), exit.getX() - center.getX());

  const float entranceAngle = exitAngle - degreesToRadians(turn);

  entrance.setX(center.getX() + cos(entranceAngle) * radius);

  entrance.setY(center.getY() + sin(entranceAngle) * radius);

  entrance.setHeading(entranceHeading);

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
  if (!source.getPositioned()) return false;

  Tag* destinationTag = findTagByUid(sourceTag.getConnectedTagUid());

  if (!destinationTag) return false;

  Track* destination = findTrackByUid(destinationTag->getUid());

  if (!destination || destination->getPositioned()) return false;

  const int sourceIndex = &sourceTag - source.getTags().data();

  const int destinationIndex = destinationTag - destination->getTags().data();

  // Transfer the connected endpoint position.
  Point& point = destination->getPoints()[destinationIndex];

  point = source.getPoints()[sourceIndex];

  // Reverse heading when connecting through an entrance.
  // TODO Might be able to have it so headings are correctly set and thus we dont have to worry about index issues
  point.setHeading(normalizeHeading(
      point.getHeading() + (sourceIndex % 2 == 0 ? 180.0f : 0.0f) +
      (destinationIndex % 2 != 0 ? 180.0f : 0.0f)));

  // Calculate the destination track.
  if (destinationIndex % 2 == 0) {
    calculateFromEntrance(*destination);
  } else {
    calculateFromExit(*destination);
  }

  return destination->getPositioned();
}
// ============================================================
// CALCULATE ENTIRE MAP
// ============================================================
void TrackMap::calculateMap() {
  if (tracks.empty()) {
    return;
  }

  for (Track& track : tracks) {
    track.setPositioned(false);
  }

  // Establish the origin.
  tracks[0].getPoints()[0] = Point(0.0f, 0.0f, 0.0f);

  calculateFromEntrance(tracks[0]);

  if (!tracks[0].getPositioned()) {
    Serial.println("Failed to position origin");
    return;
  }

  bool changed;

  do {
    changed = false;

    for (Track& source : tracks) {
      if (!source.getPositioned()) {
        continue;
      }

      for (Tag& tag : source.getTags()) {
        // TODO could simplify positionConnectedTrack by changing its call
        // structure?
        if (positionConnectedTrack(source, tag)) {
          changed = true;
        }
      }
    }

  } while (changed);

  for (size_t i = 0; i < tracks.size(); ++i) {
    Serial.printf("Track %u: %s\n", static_cast<unsigned>(i),
                  tracks[i].getPositioned() ? "POSITIONED" : "NOT POSITIONED");
  }
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
  if (!a || !b || a == b) {
    Serial.println("ERROR: Invalid tags");
    return false;
  }

  Track* trackA = findTrackByUid(a->getUid());
  Track* trackB = findTrackByUid(b->getUid());

  if (!trackA || !trackB) {
    Serial.println("ERROR: Could not find owning tracks");
    return false;
  }

  // Store the connection in both directions.
  a->setConnectedTagUid(b->getUid());
  b->setConnectedTagUid(a->getUid());

  trackA->setPositioned(false);
  trackB->setPositioned(false);

  Serial.println("Tags connected");

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
  if (!uid.isValid()) {
    return nullptr;
  }

  for (Track& track : tracks) {
    for (Tag& tag : track.getTags()) {
      if (tag.getUid().isValid() && tag.getUid().equals(uid)) {
        return &track;
      }
    }
  }

  return nullptr;
}

// ============================================================
// FIND TAG BY UID
// ============================================================

Tag* TrackMap::findTagByUid(Uid uid) {
  if (!uid.isValid()) {
    return nullptr;
  }

  for (Track& track : tracks) {
    for (Tag& tag : track.getTags()) {
      if (tag.getUid().isValid() && tag.getUid().equals(uid)) {
        return &tag;
      }
    }
  }

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
