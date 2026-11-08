
#include "TrackScanner.h"

// TODO fix issue where if track is scanned it does not get sent to display
// unless you connect to website

// ============================================================================
// CONSTRUCTOR
// ============================================================================

TrackScanner::TrackScanner(TrackMap& map) : trackMap(map), pathFinder(map) {
  app.setScannerMode(REGISTER_TRACKS);

  firstTagUid.clear();
  haveFirstTag = false;

  connectionTagUid.clear();
  haveConnectionTag = false;
  // use same units
}

// ============================================================================
// BEGIN
// ============================================================================

void TrackScanner::begin() {
  float diameter = 36.0;
  float length = (diameter * M_PI) / 8;
  Uid emptyUid;

  app.setTemplateTrack(trackMap.createTrack(CURVED, length, diameter / 2, 0, 0,
                                            0, true, emptyUid, emptyUid,
                                            emptyUid, emptyUid, true));

  // app.setScannerMode(REGISTER_TRACKS);

  firstTagUid.clear();
  haveFirstTag = false;

  connectionTagUid.clear();
  haveConnectionTag = false;

  Serial.println();
  Serial.println("========================================");
  Serial.println("TRACK SCANNER");
  Serial.println("========================================");
  Serial.println();

  Serial.println("Scan two NFC tags to create a STRAIGHT track.");
  Serial.println("First tag = entranceA");
  Serial.println("Second tag = exitA");
  Serial.println();

  Serial.println("When all tracks have been scanned,");
  Serial.println("send D over Serial.");
  Serial.println();

  nfc.setupNFC();
  TrackPathFinder pathFinder(trackMap);
}

// ============================================================================
// PROCESS NFC TAG
// ============================================================================

void TrackScanner::processScans() {


  JsonDocument doc;
  JsonObject tagJson = doc.to<JsonObject>();

  // Temporary UID returned by the NFC reader.
  Uid uid;

  NFCReadResult nfcReadResults = nfc.readNFC(uid, tagJson);

  if (nfcReadResults == NFC_READ_FAILED) {
    // app.setScannerStatusLabelText("Nfc Read failed");

    return;
  }

  if (!uid.isValid()) {
    app.setScannerStatusLabelText("Nfc Read failed");

    Serial.println("Invalid UID.");
    return;
  }
  // has found a uid dont know if it got the jason tho
  if (app.getScannerMode() != PATH_FINDING || app.getScannerMode() != NORMAL) {
    app.setRampTime(0);
    app.setTargetSpeed(0);
  }

  trackLocation(uid, tagJson);

  if (app.getScannerMode() == REGISTER_TRACKS &&
      nfcReadResults != NFC_READ_DUPLICATE) {
    app.setScannerStatusLabelText("Nfc tag read");

    Serial.println("registering track");

    registerScannedTracks(uid);
    app.setTargetSpeed(app.getUserSpeed());

    return;
  }

  if (app.getScannerMode() == WRITING_TRACKS) {
    app.setScannerStatusLabelText("Writing nfc tag");

    writeTag(uid, tagJson);
    app.setTargetSpeed(app.getUserSpeed());
    return;
  }

  if (app.getScannerMode() == SCANNING_TRACKS &&
      nfcReadResults == NFC_READ_SUCCESS) {
    app.setScannerStatusLabelText("Nfc tag read");

    addTrack(uid, tagJson);
    app.setTargetSpeed(app.getUserSpeed());

    return;
  }

  if (app.getScannerMode() == PATH_FINDING) {
    pathFind();
  }
}

void TrackScanner::pathFind() {
  Uid startUid;
  Uid destinationUid = app.getDestinationUid();
  bool uidIsForward = false;
  if (!forwardsUid.isValid() || !backwardsUid.isValid()) {
    return;
  }
  if (forwardsUid.isValid() && !backwardsUid.isValid()) {
    uidIsForward = true;
    startUid = forwardsUid;
  } else if (backwardsUid.isValid()) {
    uidIsForward = false;
    startUid = backwardsUid;
  }

  std::vector<Uid> path;

  int setSpeed = app.getUserSpeed();
  if (pathFinder.findPath(startUid, destinationUid, path)) {
    if (forwardsUid.isValid() && path[0].equals(forwardsUid) && uidIsForward) {
      // go forward

      app.setTargetSpeed(setSpeed);
    } else if (forwardsUid.isValid() && path[0].equals(forwardsUid) &&
               !uidIsForward) {
      // go backwards
      app.setTargetSpeed(-setSpeed);
    } else if (backwardsUid.isValid() && path[0].equals(backwardsUid) &&
               uidIsForward) {
      // go forwards
      app.setTargetSpeed(setSpeed);

    } else if (backwardsUid.isValid() && path[0].equals(backwardsUid) &&
               !uidIsForward) {
      // go backwards
      app.setTargetSpeed(-setSpeed);
    }
    app.setScannerStatusLabelText("Path found");
    // TODO find way to display found path
    for (const Uid& uid : path) {
      Serial.println(uid.toString());
    }

  } else {
    app.setScannerStatusLabelText("No path exists.");
  }
  // TODO write these functions
  // if we have a destination tag app.getDestinationUid() then we calculate
  // the path using PathFinder and use location data to decide if we should
  // move forwards or backwards
}
void TrackScanner::trackLocation(const Uid& uid, JsonObject& jsonTag) {
  app.setCurrentUid(uid);
  if (app.getCurrentSpeed() > 0) {
    // going forwards
    backwardsUid = uid;
    if (forwardsUid.equals(backwardsUid)) {
      forwardsUid = Uid();
    }
  } else {
    forwardsUid = uid;
    if (backwardsUid.equals(forwardsUid)) {
      backwardsUid = Uid();
    }
  }
  // TODO write these functions
  // reads current tag and uses that to set our current scanned uid used for
  // locating need to determine what direction the train is facing can do that
  // with a forward and backward uid tag that records what tag was in front and
  // behind based on what was scanned and how train moved
}
// ============================================================================
// WRITE TAG
// ============================================================================

void TrackScanner::writeTag(const Uid& uid, JsonObject& jsonTag) {
  if (haveFirstTag) {
    Serial.println();
    Serial.println("ERROR: You have only scanned one tag.");
    Serial.println("Scan the second tag first.");

    app.setScannerMode(app.getPreviousScannerMode());

    return;
  }

  if (trackMap.getTrackCount() == 0) {
    // Serial.println();
    // Serial.println("ERROR: No tracks have been created.");

    app.setScannerMode(app.getPreviousScannerMode());

    return;
  }

  JsonDocument newDoc;
  JsonObject newTagJson = newDoc.to<JsonObject>();

  writeTrackForTag(uid, newTagJson);

  delay(100);

  NFCWriteResult nfcWriteResults = nfc.writeNFC(newTagJson, uid);

  if (nfcWriteResults == NFC_WRITE_SUCCESS) {
    Serial.println("Track written successfully!");
  } else if (nfcWriteResults != NFC_WRITE_DUPLICATE) {
    Serial.println("Failed to write track.");
  }
}

// ============================================================================
// ADD TRACK
// ============================================================================

void TrackScanner::addTrack(const Uid& uid, JsonObject& jsonTag) {
  Serial.println("Scanning track");

  if (jsonTag.size() == 0) {
    Serial.println("json tag empty cant make track");
    return;
  }

  if (!trackMap.doesTrackExist(uid)) {
    Serial.println("creating track");
    bool success;
    // Track track;
    // track.fromCondensedJson(success,jsonTag);
    Track track = Track(success, jsonTag, true);
    track.setInclude(false);
    if (success) {
      Serial.println("Adding Track");

      app.setScannerStatusLabelText("Adding Track");

      trackMap.addTrack(track);
    } else {
      app.setScannerStatusLabelText("Track scanner failed to add track");
    }
  }

  // ============================================================
  // First connection tag
  // ============================================================

  if (!haveConnectionTag) {
    connectionTagUid = uid;
    haveConnectionTag = true;

  } else if (trackMap.doUidsBelongToSameTrack(connectionTagUid, uid)) {
    connectionTagUid = uid;
    haveConnectionTag = true;
  } else {
    Serial.println("attempting to connect");

    // ==========================================================
    // Second connection tag
    // ==========================================================

    Tag* tagConnection = trackMap.findTagByUid(connectionTagUid);

    Tag* tagCurrent = trackMap.findTagByUid(uid);

    if (!tagConnection) {
      app.setScannerStatusLabelText("Failed To Connect Tracks");

      Serial.println("Could not find connection tag.");

      haveConnectionTag = false;
      return;
    }

    if (!tagCurrent) {
      app.setScannerStatusLabelText("Failed To Connect Tracks");

      Serial.println("Could not find current tag.");

      haveConnectionTag = false;
      return;
    }

    app.setScannerStatusLabelText("Adding Track and connection tags");

    trackMap.connectTags(tagConnection, tagCurrent);

    connectionTagUid.clear();
    haveConnectionTag = false;
  }

  Serial.print("Total tracks: ");

  Serial.println(trackMap.getTrackCount());
  trackMap.calculateMap();
}

// ============================================================================
// TRACK SCANNING
// ============================================================================

void TrackScanner::registerScannedTracks(const Uid& uid) {
  if (app.getPreviousScannerMode() != REGISTER_TRACKS) {
    Serial.println("switching back to register");
    // Todo might cause error???
    firstTagUid.clear();
    haveFirstTag = false;
  }
  if (!app.getTemplateTrack()) {
    app.setScannerStatusLabelText("Error: Failed To Register Tracks");

    Serial.println("ERROR: No template track.");
    return;
  }
  app.setScannerStatusLabelText("Scanning");

  // ----------------------------------------------------------
  // First tag
  // ----------------------------------------------------------

  if (!haveFirstTag) {
    firstTagUid = uid;
    haveFirstTag = true;

    Serial.println();
    Serial.println("First tag recorded.");

    Serial.print("Entrance UID: ");
    Serial.println(firstTagUid.toString());

    Serial.println();
    Serial.println("Scan the second tag.");
    app.setScannerStatusLabelText("Success: scan second tag");

    return;
  }

  // ----------------------------------------------------------
  // Second tag
  // ----------------------------------------------------------

  if (firstTagUid.equals(uid)) {
    Serial.println();
    Serial.println("Same tag detected.");
    Serial.println("Scan a different tag.");
    app.setScannerStatusLabelText("Dup: a different tag.");

    return;
  }

  Serial.println();
  Serial.println("Second tag detected.");
  app.setScannerStatusLabelText("Second tag detected.");

  // TODO:
  // Need to have it be able to select which UID
  // exit/entrance it selects, like switches or cross. would have to add a list
  // recording unique uids seen and have user scan in certain order
  // todo have tracks cleared after registering
  // ----------------------------------------------------------
  // Reset temporary tag
  // ----------------------------------------------------------

  firstTagUid.clear();
  haveFirstTag = false;

  Serial.println();
  Serial.println("Track stored.");

  Serial.print("Total tracks: ");

  Serial.println(trackMap.getTrackCount());

  Serial.println();
  Serial.println("Scan the first tag of the next track.");
  app.setScannerStatusLabelText("Scan the first tag of the next track.");
}

// ============================================================================
// WRITE TRACK FOR TAG
// ============================================================================

bool TrackScanner::writeTrackForTag(const Uid& uid, JsonObject& jsonTag) {
  Track* track = trackMap.findTrackByUid(uid);

  if (!track) {
    Serial.println();
    Serial.println("Tag was not found in the track list.");
    app.setScannerStatusLabelText("Error: can't write to unkown track");

    return false;
  }

  track->toCondensedJson(jsonTag);

  return true;
}

// ============================================================================
// SERIAL COMMANDS
// ============================================================================

void TrackScanner::processSerial() {
  if (!Serial.available()) {
    return;
  }

  char command = Serial.read();

  if (command == 'G' || command == 'g') {
    Serial.println("scanning mode");

    app.setScannerMode(SCANNING_TRACKS);

  } else if (command == 'D' || command == 'd') {
    Serial.println("writing mode");

    app.setScannerMode(WRITING_TRACKS);
  }
}
