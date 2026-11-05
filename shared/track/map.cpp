#include "map.h"

// ============================================================
// POSITION CONNECTED TRACK
// ============================================================

bool TrackMap::positionConnectedTrack(Track& source, Tag& sourceTag) {
  if (!source.getPositioned()) return false;
  // find the track and tag that needs to get positioned
  Tag* destinationTag = findTagByUid(sourceTag.getConnectedTagUid());

  if (!destinationTag) return false;

  Track* destination = findTrackByUid(destinationTag->getUid());

  if (!destination || destination->getPositioned()) return false;

  destination->calculatePoseFromSourceTag(sourceTag, destinationTag);

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

  tracks[0].setPositioned(true);

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
                             float branchAngle, float branchLength,
                             float branchRadius, bool direction,
                             Uid entranceAUid, Uid exitAUid, Uid entranceBUid,
                             Uid exitBUid, bool dontAdd,
                             Uid connectEntranceAUid, Uid connectExitAUid,
                             Uid connectEntranceBUid, Uid connectExitBUid) {
  // cleanup call structure for this function
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

const std::vector<Track>& TrackMap::getTracks() const { return tracks; }
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
