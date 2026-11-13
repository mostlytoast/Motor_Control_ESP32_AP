#include "map.h"

// ============================================================
// POSITION CONNECTED TRACK
// ============================================================

bool TrackMap::positionConnectedTrack(Track& source, Tag& sourceTag) {
  if (!source.getPositioned()) {
    Serial.println("source track not positioned");

    return false;
  }
  // find the track and tag that needs to get positioned
  Tag* destinationTag = findTagByUid(sourceTag.getConnectedTagUid());
  // Serial.println("pos 1");

  if (!destinationTag) {
    Serial.println("failed to find destination tag");

    return false;
  }
  // Serial.println("pos 2");

  Track* destination = findTrackByUid(destinationTag->getUid());
  // Serial.println("pos 4");

  if (!destination || destination->getPositioned()) {
    Serial.println("failed to find destination track or already positioned");
    return false;
  }

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
        Serial.println("positioning track");
        Serial.println(tag.getUid().toString());
        Serial.println(tag.getConnectedTagUid().toString());

        if (positionConnectedTrack(source, tag)) {
          changed = true;
        }
      }
    }

  } while (changed);
  // for (Track& track : *tracks) {
  //   track.printTrack();
  // }
  // for (size_t i = 0; i < tracks->size(); ++i) {

  //   tracks[i].printTrack();

  //   // Serial.printf(
  //   //     "Track %u: %s\n", static_cast<unsigned>(i),
  //   //     (*tracks)[i].getPositioned() ? "POSITIONED" : "NOT POSITIONED");
  // }
}
// ==x==========================================================
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
    mapModified = true;

    // Add to the tracks array
    tracks->emplace_back(track);
    // Serial.println("added tracks");
    return &tracks->back();
  }
}

void TrackMap::addTrack(Track track) {
  mapModified = true;

  (*app.getCurrentTracks()).emplace_back(track);
}

// void TrackMap::addStation(Station station) {
//   mapModified = true;

//   (*app.getCurrentStations()).emplace_back(station);
// }

// Station* TrackMap::createStation(Uid stationUid, bool dontAdd) {
//   std::vector<Station>* stations = app.getCurrentStations();

//   Station station = Station(stationUid, dontAdd);

//   if (dontAdd) {
//     return &station;
//   } else {
//     stations->emplace_back(station);
//   }
// }
// ============================================================
// CONNECT TAGS
// ============================================================

// bool TrackMap::connectStationToTag(Station* a, Tag* b) {
//   Serial.print("connecting");

//   if (!a || !b || a->getTag() == (*b)) {
//     Serial.println("ERROR: Invalid Station or tag");
//     return false;
//   }

//   // Serial.print(a->getUid().toString());
//   // Serial.print(b->getUid().toString());

//   Track* trackA = findTrackByUid(b->getUid());

//   if (!trackA) {
//     Serial.println("ERROR: Could not find owning tracks");
//     return false;
//   }

//   // Store the connection in both directions.
//   a->getTag().setConnectedTagUid(b->getUid());

//   trackA->setPositioned(false);
//   mapModified = true;
//   Serial.println("Station and Tag connected");

//   return true;
// }

bool TrackMap::connectTags(Tag* a, Tag* b) {
  // Serial.print("connecting");

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
  mapModified = true;
  // Serial.println("Tags connected");

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
// auto* TrackMap::findObjectByUid(Uid uid) {
//   if (findTrackByUid(uid) != nullptr) {
//     return findTrackByUid(uid);
//   }

//   if (findStationByUid(uid) != nullptr) {
//     return findStationByUid(uid);
//   }
// }
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

// Station* TrackMap::findStationByUid(Uid uid) {
//   if (!uid.isValid()) {
//     return nullptr;
//   }

//   for (Station& Station : (*app.getCurrentStations())) {
//     if (Station.getTag().getUid().isValid() &&
//         Station.getTag().getUid().equals(uid)) {
//       return &Station;
//     }
//   }

//   return nullptr;
// }

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

  // for (Station& station : (*app.getCurrentStations())) {
  //   if (station.getTag().getUid().isValid() &&
  //       station.getTag().getUid().equals(uid)) {
  //     return &station.getTag();
  //   }
  // }

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
  mapModified = true;
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
  mapModified = true;
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
        if (pastTrack == currentTrack) {
          existsInPast = true;
        }
      }
      if (!existsInPast) {
        // Serial.println("found tracks to add");
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
      // Serial.println("found tracks to remove");

      JsonObject obj = removeUidsFromMap.add<JsonObject>();
      obj["uid"] = pastUid.toString();
    }
  }
}

bool TrackMap::removeTrack(const Track* track) {
  mapModified = true;
  std::vector<Track>* tracks = app.getCurrentTracks();

  auto it = std::find_if(tracks->begin(), tracks->end(),
                         [track](Track& p) { return p == *track; });

  if (it != tracks->end()) {
    tracks->erase(it);
    return true;
  }
  return false;
}

void TrackMap::condensedJsonToMap(JsonArray& addTracksJson,
                                  // JsonArray& addStationsJson,
                                  JsonArray& removeUidsFromMap) {
  std::vector<Track>* tracks = app.getCurrentTracks();

  // todo find better way to prevent it creating multiple of same track

  if (addTracksJson.isNull()) {
    Serial.println("JsonToMap: no tracks array found");
    return;
  }

  for (const JsonObject& addTrackJson : addTracksJson) {
    if (tracks->size() >= MAX_TRACKS) {
      continue;
    } else {
      // Serial.println("adding tracks");

      // Add to the tracks

      bool success;
      Track track = Track(success, addTrackJson, true);
      if (success) {
        Track* existingTrack = findTrackByUid(track.getTags()[0].getUid());
        if (existingTrack != nullptr) {
          removeTrack(existingTrack);
        }
        mapModified = true;
        tracks->emplace_back(track);
        // Serial.println("successfully added tracks");
      }
    }
  }

  for (const JsonObject& removeUid : removeUidsFromMap) {
    if (tracks->size() >= MAX_TRACKS) {
      continue;
    } else if (removeUid["uid"].is<const char*>()) {
      mapModified = true;
      // Remove track from tracks
      Uid uid(removeUid["uid"].as<const char*>());
      const Track* track = findTrackByUid(uid);
      removeTrack(track);
    }
  }

  // Serial.println("map done");
  app.setPastTracks(*tracks);

  trackMap.calculateMap();
}

void TrackMap::clearMap() {
  // Serial.println("map clear");

  mapModified = true;
  (*app.getCurrentTracks()).clear();
  // (*app.getCurrentStations()).clear();
}

bool TrackMap::getMapModified() { return mapModified; }
void TrackMap::setMapModified(bool mapModified_) { mapModified = mapModified_; }
