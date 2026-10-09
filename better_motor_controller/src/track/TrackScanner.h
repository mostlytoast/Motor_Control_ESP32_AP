
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../hardware/NFC.h"
#include "TrackScanner.h"
#include "../../shared/track/map.h"
#include "../../shared/utils/AppState.h"
#include "../../shared/utils/uidLogic.h"
class TrackScanner {
 public:
  TrackScanner(TrackMap& map);

  // Start scanning tracks.
  void begin();

  // Give a newly scanned NFC UID to the scanner.
  void processScans();

  // Process commands such as 'D'.
  void processSerial();

  // Current scanner mode.
  bool isWritingMode() const;

 private:
  static constexpr uint8_t MAX_TRACKS = 100;

  Track tracks[MAX_TRACKS];
  uint8_t trackCount = 0;
  TrackMap& trackMap;
  NFC nfc;

  // First tag of the current track.
  Uid firstTagUid;

  bool haveFirstTag;

  Uid connectionTagUid;

  bool haveConnectionTag;

  // Write the track associated with a tag.
  //   todo rename
  bool writeTrackForTag(const Uid& uid, JsonObject& jsonTag);
  void addTrack(const Uid& uid, JsonObject& jsonTag);
  // Track creation.
  void registerScannedTracks(const Uid& uid, Track* templateTrack);

  void writeTag(const Uid& uid, JsonObject& jsonTag);
  // Track lookup.
  Track* findTrackByUid(Uid uid);

  // JSON.

  // UID helpers.

  // void copyUid(uint8_t* destination, const uint8_t* source);
};
extern TrackMap trackMap;
extern TrackScanner trackScanner;