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
  std::vector<Track>* tracks = app.getCurrentTracks();

  if (tracks == nullptr || tracks->empty()) {
    return;
  }

  for (Track& track : *tracks) {
    track.setPositioned(false);
  }
  (*tracks)[0].setPositioned(true);

  bool changed;

  do {
    changed = false;

    for (Track& source : *tracks) {
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

  for (size_t i = 0; i < tracks->size(); ++i) {
    Serial.printf(
        "Track %u: %s\n", static_cast<unsigned>(i),
        (*tracks)[i].getPositioned() ? "POSITIONED" : "NOT POSITIONED");
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
  std::vector<Track>* tracks = app.getCurrentTracks();

  Track track = Track(type, length, radius, branchAngle, branchRadius,
                      branchLength, direction, entranceAUid, exitAUid,
                      entranceBUid, exitBUid, dontAdd, connectEntranceAUid,
                      connectExitAUid, connectEntranceBUid, connectExitBUid);
  if (dontAdd) {
    // Use temporary storage without changing tracks->size()

    tempTrack = track;

    return &tempTrack;

  } else if (tracks == nullptr || tracks->size() >= MAX_TRACKS && !dontAdd) {
    return nullptr;
  } else {
    trackModified = true;

    // Add to the tracks array
    tracks->emplace_back(track);
    Serial.println("added tracks");
    return &tracks->back();
  }
}

void TrackMap::addTrack(Track track) {
  trackModified = true;

  (*app.getCurrentTracks()).emplace_back(track);
}

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
  Serial.print("connecting");

  if (!a || !b || a == b) {
    Serial.println("ERROR: Invalid tags");
    return false;
  }

  Serial.print(a->getUid().toString());
  Serial.print(b->getUid().toString());

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
  trackModified = true;
  Serial.println("Tags connected");

  return true;
}

const std::vector<Track>* TrackMap::getTracks() const {
  return app.getCurrentTracks();
}
// ============================================================
// GET TRACK
// ============================================================
// TODO remove

// ============================================================
// FIND TRACK
// ============================================================

Track* TrackMap::findTrackByUid(Uid uid) {
  if (!uid.isValid()) {
    return nullptr;
  }

  for (Track& track : (*app.getCurrentTracks())) {
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

  for (Track& track : (*app.getCurrentTracks())) {
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

uint8_t TrackMap::getTrackCount() const {
  return (*app.getCurrentTracks()).size();
}

void TrackMap::mapToJson(JsonDocument& doc) {
  trackModified = true;
  std::vector<Track>* tracks = app.getCurrentTracks();

  JsonArray tracksJson = doc["tracks"].to<JsonArray>();
  int count = 0;
  for (uint8_t i = 0; i < tracks->size(); i++) {
    JsonObject obj = tracksJson.add<JsonObject>();

    Track& track = (*tracks)[i];

    if (track.getInclude()) {
      obj["id"] = count;

      track.toJson(obj);
      count++;
    }
  }
}

void TrackMap::JsonToMap(JsonArray doc) {
  trackModified = true;
  // todo find better way to prevent it creating multiple of same track
  // if (tracks->size() ==1 ) return;
  clearMap();
  std::vector<Track>* tracks = app.getCurrentTracks();

  if (doc.isNull()) {
    Serial.println("JsonToMap: no tracks array found");
    return;
  }

  for (uint8_t i = 0; i < doc.size(); i++) {
    if (tracks->size() >= MAX_TRACKS) {
      continue;
    } else {
      // Add to the tracks array
      bool success;
      Track track = Track(success, doc[i].as<JsonObject>());
      if (success) tracks->emplace_back(track);
    }
  }
  trackMap.calculateMap();
}

void TrackMap::mapToCondensedJson(JsonArray& addTracksJson,
                                  JsonArray& removeUidsFromMap) {
  // Use a vector to track which UIDs are present in the current update
  std::vector<Track>* tracks = app.getCurrentTracks();
  std::vector<Track> pastTracks = app.getPastTracks();
  std::vector<Uid> currentTracksUids;

  // 1. Build the current map and populate IDs
  // 2. Identify Additions (In currentTracksUids but not in pastTracks)
  for (Track& currentTrack : *tracks) {
    currentTracksUids.emplace_back(currentTrack.getTags()[0].getUid());

    if (currentTrack.getInclude()) {
      bool existsInPast = false;
      for (Track& pastTrack : pastTracks) {
        if (pastTrack.getTags()[0].getUid().equals(
                currentTrack.getTags()[0].getUid())) {
          existsInPast = true;
        }
      }
      if (!existsInPast) {
        Serial.println("found tracks to add");
        JsonObject obj = addTracksJson.add<JsonObject>();
        currentTrack.toCondensedJson(obj);
      }
    }
  }
  // 2. Identify Removals (In pastTracks but not in currentTracksUids)

  for (Track& pastTrack : pastTracks) {
    Uid pastUid = pastTrack.getTags()[0].getUid();
    bool existsInCurrent = false;

    for (Uid currentUid : currentTracksUids) {
      if (pastUid.equals(currentUid)) {
        existsInCurrent = true;
      }
    }

    if (!existsInCurrent) {
      Serial.println("found tracks to remove");

      JsonObject obj = removeUidsFromMap.add<JsonObject>();
      obj["uid"] = pastUid.toString();
    }
  }
}

bool TrackMap::removeTrack(const Track* track) {
  trackModified = true;
  std::vector<Track>* tracks = app.getCurrentTracks();

  auto it = std::find_if(tracks->begin(), tracks->end(),
                         [track](Track& p) { return p == *track; });

  if (it != tracks->end()) {
    tracks->erase(it);
    return true;
  }
  return false;
}

void TrackMap::connectTrack(Track* track) {
  if (track == nullptr || track->getTags().size() == 0) {
    return;
  }
  for (Tag& tag : track->getTags()) {
    if (tag.getConnectedTagUid().isValid() &&
        findTagByUid(tag.getConnectedTagUid()) != nullptr) {
      connectTags(&tag, findTagByUid(tag.getConnectedTagUid()));
      Serial.println("connected tag");
    }
  }
}

void TrackMap::condensedJsonToMap(JsonArray& addTracksJson,
                                  JsonArray& removeUidsFromMap) {
  std::vector<Track>* tracks = app.getCurrentTracks();
  app.setPastTracks(*tracks);

  // todo find better way to prevent it creating multiple of same track

  if (addTracksJson.isNull()) {
    Serial.println("JsonToMap: no tracks array found");
    return;
  }

  for (const JsonObject& addTrackJson : addTracksJson) {
    if (tracks->size() >= MAX_TRACKS) {
      continue;
    } else {
      Serial.println("adding tracks");

      // Add to the tracks

      bool success;
      Track track = Track(success, addTrackJson, true);
      if (success) {
        trackModified = true;
        tracks->emplace_back(track);
        Serial.println("successfully added tracks");
      }
    }
  }

  for (const JsonObject& removeUid : removeUidsFromMap) {
    if (tracks->size() >= MAX_TRACKS) {
      continue;
    } else if (removeUid["uid"].is<const char*>()) {
      trackModified = true;
      // Remove track from tracks
      Uid uid(removeUid["uid"].as<const char*>());
      const Track* track = findTrackByUid(uid);
      removeTrack(track);
    }
  }

  trackMap.calculateMap();
}

void TrackMap::clearMap() {
  trackModified = true;
  (*app.getCurrentTracks()).clear();
}

bool TrackMap::getTracksModified() { return trackModified; }
void TrackMap::setTracksModified(bool trackModified_) {
  trackModified = trackModified_;
}
