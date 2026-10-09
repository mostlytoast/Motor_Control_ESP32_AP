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

  return Point(start.x + cos(radians) * distance,
               start.y + sin(radians) * distance);
}

static Point rotatePoint(Point point, float heading) {
  float radians = degreesToRadians(heading);
  float c = cos(radians);
  float s = sin(radians);

  return Point(point.x * c - point.y * s, point.x * s + point.y * c);
}

// ============================================================
// TAG HELPERS
// ============================================================

static bool isEntrance(Track& track, Tag* tag) {
  return tag == &track.entranceA || tag == &track.entranceB;
}

static bool isExit(Track& track, Tag* tag) {
  return tag == &track.exitA || tag == &track.exitB;
}

// ============================================================
// STRAIGHT TRACK
// ============================================================

void TrackMap::calculateStraightPose(Track& track) {
  track.pose.entranceAHeading = normalizeHeading(track.pose.entranceAHeading);

  track.pose.exit =
      movePoint(track.pose.entrance, track.pose.entranceAHeading, track.length);

  track.pose.exitAHeading = track.pose.entranceAHeading;

  track.positioned = true;
}

void TrackMap::calculateStraightFromExit(Track& track) {
  track.pose.exitAHeading = normalizeHeading(track.pose.exitAHeading);

  track.pose.entrance =
      movePoint(track.pose.exit, track.pose.exitAHeading, -track.length);

  track.pose.entranceAHeading = track.pose.exitAHeading;

  track.positioned = true;
}

// ============================================================
// CALCULATE FROM ENTRANCE
// ============================================================

void TrackMap::calculateFromEntrance(Track& track) {
  switch (track.trackType) {
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
      break;
  }
}

// ============================================================
// CALCULATE FROM EXIT
// ============================================================

void TrackMap::calculateFromExit(Track& track) {
  switch (track.trackType) {
    case STRAIGHT:
    case BUMPER:
    case SWITCH:
    case CROSS:
      calculateStraightFromExit(track);
      break;

    case CURVED:
      calculateCurveFromExit(track);
      break;
  }
}

// ============================================================
// GENERAL TRACK POSE
// ============================================================

void TrackMap::calculateTrackPose(Track& track) {
  calculateFromEntrance(track);
}

// ============================================================
// CURVE FROM ENTRANCE
// ============================================================

void TrackMap::calculateCurvePose(Track& track) {
  float radius = track.radius;

  if (radius <= 0.0f) {
    calculateStraightPose(track);
    return;
  }

  float angleRadians = track.length / radius;
  float angleDegrees = radiansToDegrees(angleRadians);

  float turn = track.direction ? -angleDegrees : angleDegrees;

  float heading = normalizeHeading(track.pose.entranceAHeading);

  float centerHeading = track.direction ? heading - 90.0f : heading + 90.0f;

  Point center = movePoint(track.pose.entrance, centerHeading, radius);

  float startAngle =
      atan2(track.pose.entrance.y - center.y, track.pose.entrance.x - center.x);

  float endAngle = startAngle + degreesToRadians(turn);

  track.pose.exit = {center.x + cos(endAngle) * radius,
                     center.y + sin(endAngle) * radius};

  track.pose.exitAHeading = normalizeHeading(heading + turn);

  track.positioned = true;
}

// ============================================================
// CURVE FROM EXIT
// ============================================================

void TrackMap::calculateCurveFromExit(Track& track) {
  float radius = track.radius;

  if (radius <= 0.0f) {
    calculateStraightFromExit(track);
    return;
  }

  float angleRadians = track.length / radius;
  float angleDegrees = radiansToDegrees(angleRadians);

  float turn = track.direction ? -angleDegrees : angleDegrees;

  float exitHeading = normalizeHeading(track.pose.exitAHeading);

  float entranceHeading = normalizeHeading(exitHeading - turn);

  float centerHeading =
      track.direction ? exitHeading - 90.0f : exitHeading + 90.0f;

  Point center = movePoint(track.pose.exit, centerHeading, radius);

  float exitAngle =
      atan2(track.pose.exit.y - center.y, track.pose.exit.x - center.x);

  float entranceAngle = exitAngle - degreesToRadians(turn);

  track.pose.entrance = Point(center.x + cos(entranceAngle) * radius,
                              center.y + sin(entranceAngle) * radius);

  track.pose.entranceAHeading = entranceHeading;

  track.positioned = true;
}

// ============================================================
// SWITCH POSE
// ============================================================

void TrackMap::calculateSwitchPose(Track& track) {
  float heading = normalizeHeading(track.pose.entranceAHeading);

  // Main route
  track.pose.exit = movePoint(track.pose.entrance, heading, track.length);

  track.pose.exitAHeading = heading;

  // Curved branch
  float angle = degreesToRadians(track.branchAngle);

  float radius = track.branchRadius;

  Point local = Point(radius * sin(angle), radius * (1.0f - cos(angle)));

  Point offset = rotatePoint(local, heading);

  track.pose.exitB =
      Point(track.pose.entrance.x + offset.x, track.pose.entrance.y + offset.y);

  track.pose.exitBHeading =
      normalizeHeading(heading + track.branchAngle + 10.0f);

  track.positioned = true;
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
  if (!findTagByUid(sourceTag.connectedTagUid)) return false;

  Tag* destinationTag = findTagByUid(sourceTag.connectedTagUid);

  Track* destinationTrack = destinationTag->track;

  if (!destinationTrack) return false;

  if (destinationTrack->positioned) return true;

  bool sourceIsEntrance = isEntrance(source, &sourceTag);

  bool sourceIsExit = isExit(source, &sourceTag);

  if (!sourceIsEntrance && !sourceIsExit) return false;

  Point sourcePoint;
  float sourceHeading;

  if (sourceIsEntrance) {
    sourcePoint = source.pose.entrance;

    sourceHeading = source.pose.entranceAHeading;

  } else {
    sourcePoint = source.pose.exit;

    sourceHeading = source.pose.exitAHeading;
  }

  sourceHeading = normalizeHeading(sourceHeading);

  bool destinationIsEntrance = isEntrance(*destinationTrack, destinationTag);

  bool destinationIsExit = isExit(*destinationTrack, destinationTag);

  if (!destinationIsEntrance && !destinationIsExit) {
    return false;
  }

  float headingAwayFromSource = sourceIsEntrance
                                    ? normalizeHeading(sourceHeading + 180.0f)
                                    : sourceHeading;

  if (destinationIsEntrance) {
    destinationTrack->pose.entrance = sourcePoint;

    destinationTrack->pose.entranceAHeading = headingAwayFromSource;

    calculateFromEntrance(*destinationTrack);

    return destinationTrack->positioned;
  }

  destinationTrack->pose.exit = sourcePoint;

  destinationTrack->pose.exitAHeading =
      normalizeHeading(headingAwayFromSource + 180.0f);

  calculateFromExit(*destinationTrack);

  return destinationTrack->positioned;
}

// ============================================================
// CALCULATE ENTIRE MAP
// ============================================================

void TrackMap::calculateMap() {
  // Serial.println("calculating map");
  if (trackCount == 0) return;

  for (uint8_t i = 0; i < trackCount; i++) {
    tracks[i].positioned = false;
  }

  // Track 0 is the world origin.
  tracks[0].pose.entrance = Point(0.0f, 0.0f);

  tracks[0].pose.entranceAHeading = 0.0f;

  calculateFromEntrance(tracks[0]);

  bool changed;

  do {
    changed = false;

    for (uint8_t i = 0; i < trackCount; i++) {
      Track& track = tracks[i];

      Serial.printf("Track %d: entranceA=%p entranceB=%p exitA=%p exitB=%p\n",
                    i, findTagByUid(track.entranceA.connectedTagUid),
                    findTagByUid(track.entranceB.connectedTagUid),
                    findTagByUid(track.exitA.connectedTagUid),
                    findTagByUid(track.exitB.connectedTagUid));

      if (!track.positioned) continue;

      Tag* tags[] = {&track.entranceA, &track.entranceB, &track.exitA,
                     &track.exitB};

      for (Tag* tag : tags) {
        if (!findTagByUid(tag->connectedTagUid)) continue;

        Track* next = findTagByUid(tag->connectedTagUid)->track;

        if (!next || next->positioned) continue;

        if (positionConnectedTrack(track, *tag)) {
          changed = true;
        }
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
  if (trackCount >= MAX_TRACKS && !dontAdd) return nullptr;

  Track* track;

  if (dontAdd) {
    // Use temporary storage without changing trackCount
    track = &tempTrack;
  } else {
    // Add to the tracks array
    track = &tracks[trackCount++];
  }

  track->length = length;
  track->radius = radius;
  track->branchAngle = branchAngle;
  track->branchRadius = branchRadius;
  track->branchLength = branchLength;
  track->direction = direction;
  track->trackType = type;
  track->include = !dontAdd;
  track->pose = {};
  track->positioned = false;

  Tag* tags[] = {&track->entranceA, &track->entranceB, &track->exitA,
                 &track->exitB};

  // Uid connectedTagUid[] = {connectEntranceAUid, connectExitAUid,
  //                          connectEntranceBUid, connectExitBUid};

  for (Tag* tag : tags) {
    tag->track = track;

    // tag->connectedTag = nullptr;
  }

  track->entranceA.connectedTagUid = connectEntranceAUid;
  track->exitA.connectedTagUid = connectExitAUid;
  track->entranceB.connectedTagUid = connectEntranceBUid;
  track->exitB.connectedTagUid = connectExitBUid;

  track->entranceA.uid = entranceAUid;
  track->exitA.uid = exitAUid;
  track->entranceB.uid = entranceBUid;
  track->exitB.uid = exitBUid;

  return track;
}

// ============================================================
// CONNECT TAGS
// ============================================================

bool TrackMap::connectTags(Tag* a, Tag* b) {
  if (!a || !b) {
    Serial.println("one of the tags is null");
    return false;
  }

  // a->connectedTag = b;
  a->connectedTagUid = b->uid;
  // b->connectedTag = a;
  b->connectedTagUid = a->uid;

  if (a->track) {
    a->track->positioned = false;
  }

  if (b->track) {
    b->track->positioned = false;
  }
  Serial.println("Connected");

  return true;
}

// ============================================================
// GET TRACK
// ============================================================

Track* TrackMap::getTrack(uint8_t index) {
  if (index >= trackCount) return nullptr;

  return &tracks[index];
}

// ============================================================
// FIND TRACK
// ============================================================

Track* TrackMap::findTrackByUid(Uid uid) {
  Tag* tag = findTagByUid(uid);

  if (tag != nullptr) {
    return tag->track;
  }

  return nullptr;
}

// ============================================================
// FIND TAG BY UID
// ============================================================

Tag* TrackMap::findTagByUid(Uid uid) {
  for (uint8_t i = 0; i < trackCount; i++) {
    Track& trackSearch = tracks[i];

    Tag* tags[] = {&trackSearch.entranceA, &trackSearch.entranceB,
                   &trackSearch.exitA, &trackSearch.exitB};

    for (Tag* tagSearch : tags) {
      // Serial.print("Stored tag UID: ");
      // Serial.println(tagSearch->uid.toString());

      // Serial.print("Searching UID: ");
      // Serial.println(uid.toString());

      if (tagSearch->uid.equals(uid)) {
        // Serial.println("found tag");

        return tagSearch;
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

  Tag* tags[] = {&track->entranceA, &track->entranceB, &track->exitA,
                 &track->exitB};
  for (Tag* tagSearch : tags) {
    if (tagSearch->uid.equals(uidB)) {
      return true;
    }
  }
  return false;
}

void TrackMap::printTrack(Track& track) {
  JsonDocument doc;

  JsonObject jsonTag = doc.to<JsonObject>();
  trackToJson(track, jsonTag);

  serializeJsonPretty(jsonTag, Serial);
}

bool TrackMap::doesTrackExist(Uid uid) { return findTagByUid(uid) != nullptr; }

// ============================================================
// GET TRACK COUNT
// ============================================================

uint8_t TrackMap::getTrackCount() const { return trackCount; }

// ============================================================
// TRACK TYPE TO STRING
// ============================================================

const char* TrackMap::trackTypeToString(TrackType type) {
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

// ============================================================
// TO JSON
// ============================================================

void TrackMap::trackToJson(Track& track, JsonObject& obj) {
  obj["length"] = track.length;

  obj["radius"] = track.radius;

  obj["direction"] = track.direction;

  obj["branchAngle"] = track.branchAngle;

  obj["branchRadius"] = track.branchRadius;

  obj["branchLength"] = track.branchLength;

  obj["type"] = trackTypeToString(track.trackType);

  JsonObject pose = obj["pose"].to<JsonObject>();

  pose["entranceAX"] = track.pose.entrance.x;

  pose["entranceAY"] = track.pose.entrance.y;

  pose["exitAX"] = track.pose.exit.x;

  pose["exitAY"] = track.pose.exit.y;

  pose["entranceAHeading"] = track.pose.entranceAHeading;

  pose["exitAHeading"] = track.pose.exitAHeading;

  pose["entranceBX"] = track.pose.entranceB.x;

  pose["entranceBY"] = track.pose.entranceB.y;

  pose["exitBX"] = track.pose.exitB.x;

  pose["exitBY"] = track.pose.exitB.y;

  pose["entranceBHeading"] = track.pose.entranceBHeading;

  pose["exitBHeading"] = track.pose.exitBHeading;

  pose["positioned"] = track.positioned;

  JsonArray tags = obj["tags"].to<JsonArray>();

  addTagJson(tags, "entranceA", track.entranceA);

  addTagJson(tags, "entranceB", track.entranceB);

  addTagJson(tags, "exitA", track.exitA);

  addTagJson(tags, "exitB", track.exitB);
}

void TrackMap::mapToJson(JsonDocument& doc) {
  JsonArray tracksJson = doc["tracks"].to<JsonArray>();
  int count = 0;
  for (uint8_t i = 0; i < trackCount; i++) {
    JsonObject obj = tracksJson.add<JsonObject>();

    Track& track = tracks[i];

    if (track.include) {
      obj["id"] = count;

      trackToJson(track, obj);
      count++;
    }
  }
  // Serial.println("tracks json");
  // serializeJsonPretty(tracksJson,Serial);
}

// ============================================================
// TAG JSON
// ============================================================

void TrackMap::addTagJson(JsonArray& array, const char* name, Tag& tag) {
  JsonObject obj = array.add<JsonObject>();

  obj["name"] = name;

  // Uid now knows its own length.
  obj["uid"] = tag.uid.toString();

  // Store whether the UID is valid.
  obj["valid"] = tag.uid.isValid();

  Uid emptyUid;
  obj["connectedTagUid"] = tag.connectedTagUid.toString();
}

// ============================================================
// FIND TRACK INDEX
// ============================================================

int TrackMap::findTrackIndex(Track* track) {
  if (!track) return -1;

  for (uint8_t i = 0; i < trackCount; i++) {
    if (&tracks[i] == track) {
      return i;
    }
  }

  return -1;
}
void TrackMap::clearMap() {
  trackCount = 0;
  memset(tracks, 0, sizeof(tracks));
  trackCount = 0;
}
void TrackMap::JsonToMap(JsonArray doc) {
  // todo find better way to prevent it creating multiple of same track
  // if (trackCount ==1 ) return;
  clearMap();
  if (doc.isNull()) {
    Serial.println("JsonToMap: no tracks array found");
    return;
  }
  // Serial.println("JsonToMap: loading in map");
  // Serial.print("JsonToMap: size ");
  // Serial.println(doc.size());
  // serializeJsonPretty(doc, Serial);
  for (uint8_t i = 0; i < doc.size(); i++) {
    jsonToTrack(doc[i]);
  }
  trackMap.calculateMap();
}

// ============================================================
// JSON TO TRACK
// ============================================================

Track* TrackMap::jsonToTrack(JsonObject obj) {
  // ============================================================
  // Validate input
  // ============================================================

  if (trackCount >= MAX_TRACKS) return nullptr;

  if (obj.isNull()) {
    Serial.println("jsonToTrack: JSON object is null");

    return nullptr;
  }

  Serial.println("=== jsonToTrack ===");

  // serializeJsonPretty(obj, Serial);

  Serial.println();

  float length = obj["length"].is<float>() ? obj["length"].as<float>() : 0;
  // Serial.println("length");
  // Serial.println(length);

  float radius = obj["radius"].is<float>() ? obj["radius"].as<float>() : 0;

  bool direction =
      obj["direction"].is<bool>() ? obj["direction"].as<bool>() : false;

  float branchAngle =
      obj["branchAngle"].is<float>() ? obj["branchAngle"].as<float>() : 0;

  float branchRadius =
      obj["branchRadius"].is<float>() ? obj["branchRadius"].as<float>() : 0;

  float branchLength =
      obj["branchLength"].is<float>() ? obj["branchLength"].as<float>() : 0;

  // ============================================================
  // Track type
  // ============================================================

  const char* type = obj["type"].as<const char*>();

  TrackType trackType = STRAIGHT;

  if (type == nullptr) {
    type = "STRAIGHT";
  }

  if (strcmp(type, "STRAIGHT") == 0) {
    trackType = STRAIGHT;

  } else if (strcmp(type, "CURVED") == 0) {
    trackType = CURVED;

  } else if (strcmp(type, "SWITCH") == 0) {
    trackType = SWITCH;

  } else if (strcmp(type, "BUMPER") == 0) {
    trackType = BUMPER;

  } else if (strcmp(type, "CROSS") == 0) {
    trackType = CROSS;

  } else {
    Serial.print("jsonToTrack: Unknown track type: ");

    Serial.println(type);

    return nullptr;
  }

  // ============================================================
  // UID objects
  // ============================================================

  Uid entranceAUid;
  Uid exitAUid;
  Uid entranceBUid;
  Uid exitBUid;

  Uid connectEntranceAUid;
  Uid connectExitAUid;
  Uid connectEntranceBUid;
  Uid connectExitBUid;

  JsonArray tags = obj["tags"].as<JsonArray>();

  if (!tags.isNull()) {
    for (JsonObject tagObj : tags) {
      const char* name = tagObj["name"].as<const char*>();

      const char* uidString = tagObj["uid"].as<const char*>();

      const char* connectedTagUid = tagObj["connectedTagUid"].as<const char*>();

      if (name == nullptr) {
        continue;
      }

      if (uidString == nullptr) {
        uidString = "";
      }

      // --------------------------------------------------------
      // Parse corresponding UID
      // --------------------------------------------------------

      if (strcmp(name, "entranceA") == 0) {
        entranceAUid.fromString(uidString);
        connectEntranceAUid.fromString(connectedTagUid);
      } else if (strcmp(name, "entranceB") == 0) {
        entranceBUid.fromString(uidString);
        connectEntranceBUid.fromString(connectedTagUid);

      } else if (strcmp(name, "exitA") == 0) {
        exitAUid.fromString(uidString);
        connectExitAUid.fromString(connectedTagUid);

      } else if (strcmp(name, "exitB") == 0) {
        exitBUid.fromString(uidString);
        connectExitBUid.fromString(connectedTagUid);

      } else {
        Serial.print("jsonToTrack: Unknown tag name: ");

        Serial.println(name);
        continue;
      }
    }
  }

  // ============================================================
  // CREATE TRACK
  // ============================================================
  // todo might need to find way to add back in pose and position marker
  // settings?  dont want stateJson to call calculateMap?
  return createTrack(trackType, length, radius, branchAngle, branchRadius,
                     branchLength, direction, entranceAUid, exitAUid,
                     entranceBUid, exitBUid, false, connectEntranceAUid,
                     connectExitAUid, connectEntranceBUid, connectExitBUid);
}