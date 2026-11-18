#include "map.h"

// ============================================================
// POSITION CONNECTED TRACK
// ============================================================

bool TrackMap::positionConnectedTrack(Track& source, Tag& sourceTag) {
  if (!source.getPositioned()) {
    Serial.println("source track not positioned");

    return false;
  }
  // source.printTrack();
  // find the track and tag that needs to get positioned
  Tag* destinationTag =
      app.getCurrentTracks()->findTagByUid(sourceTag.getConnectedTagUid());
  // Serial.println("pos 1");

  if (!destinationTag) {
    // Serial.println("failed to find destination tag");

    return false;
  }
  // Serial.println("pos 2");

  Track* destination =
      app.getCurrentTracks()->findTrackByUid(destinationTag->getUid());
  // Serial.println("pos 4");

  if (!destination || destination->getPositioned()) {
    // Serial.println("failed to find destination track or already positioned");
    return false;
  }

  destination->calculatePoseFromSourceTag(sourceTag, destinationTag);

  return destination->getPositioned();
}
// ============================================================
// CALCULATE ENTIRE MAP
// ============================================================
void TrackMap::calculateMap() {
  Tracks* tracks = app.getCurrentTracks();

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
        // Serial.println("positioning track");
        // Serial.println(tag.getUid().toString());
        // Serial.println(tag.getConnectedTagUid().toString());

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
  Tracks* tracks = app.getCurrentTracks();

  Track track = Track(type, length, radius, branchAngle, branchRadius,
                      branchLength, direction, entranceAUid, exitAUid,
                      entranceBUid, exitBUid, dontAdd, connectEntranceAUid,
                      connectExitAUid, connectEntranceBUid, connectExitBUid);
  if (dontAdd) {
    // Use temporary storage without changing tracks->size()

    tempTrack = track;

    return &tempTrack;

  } else if (tracks == nullptr || tracks->isFull() && !dontAdd) {
    return nullptr;
  } else {
    // Add to the tracks array
    tracks->emplace_back(track);
    // Serial.println("added tracks");
    return &tracks->back();
  }
}
bool TrackMap::connectUid(Uid a, Uid b) {
  Tag* tagA = app.getCurrentTracks()->findTagByUid(a);
  Tag* tagB = app.getCurrentTracks()->findTagByUid(b);
  return connectTags(tagA, tagB);
}

bool TrackMap::connectTags(Tag* a, Tag* b) {

  if (!a || !b || a == b) {
    Serial.println("ERROR: Invalid tags");
    return false;
  }

  Track* trackA = app.getCurrentTracks()->findTrackByUid(a->getUid());
  Track* trackB = app.getCurrentTracks()->findTrackByUid(b->getUid());

  if (!trackA || !trackB) {
    Serial.println("ERROR: Could not find owning tracks");
    return false;
  }

  // Store the connection in both directions.
  a->setConnectedTagUid(b->getUid());
  b->setConnectedTagUid(a->getUid());

  trackA->setPositioned(false);
  trackB->setPositioned(false);

  // Serial.println("Tags connected");

  return true;
}

void TrackMap::mapToJson(JsonDocument& doc) {
  Tracks* tracks = app.getCurrentTracks();

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

void TrackMap::jsonToMap(JsonArray doc) {
  // todo find better way to prevent it creating multiple of same track
  // if (tracks->size() ==1 ) return;
  clearMap();
  Tracks* tracks = app.getCurrentTracks();

  if (doc.isNull()) {
    Serial.println("JsonToMap: no tracks array found");
    return;
  }

  for (uint8_t i = 0; i < doc.size(); i++) {
    if (tracks->isFull()) {
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


void TrackMap::mapToCondensedJson(JsonDocument& doc) {
  Tracks* tracks = app.getCurrentTracks();

  JsonArray tracksJson = doc["tracks"].to<JsonArray>();
  int count = 0;
  for (uint8_t i = 0; i < tracks->size(); i++) {
    JsonObject obj = tracksJson.add<JsonObject>();

    Track& track = (*tracks)[i];

    if (track.getInclude()) {
      obj["id"] = count;

      track.toCondensedJson(obj);
      count++;
    }
  }
}
void TrackMap::condensedJsonToMap(JsonArray doc) {
  // todo find better way to prevent it creating multiple of same track
  // if (tracks->size() ==1 ) return;
  clearMap();
  Tracks* tracks = app.getCurrentTracks();

  if (doc.isNull()) {
    Serial.println("JsonToMap: no tracks array found");
    return;
  }

  for (uint8_t i = 0; i < doc.size(); i++) {
    if (tracks->isFull()) {
      continue;
    } else {
      // Add to the tracks array
      bool success;
      Track track = Track(success, doc[i].as<JsonObject>(), true);
      if (success) tracks->emplace_back(track);
    }
  }
  trackMap.calculateMap();
}

void TrackMap::clearMap() {
  (*app.getCurrentTracks()).clear();
}
