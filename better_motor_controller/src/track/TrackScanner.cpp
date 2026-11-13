
#include "TrackScanner.h"

TrackMap internalTrackMap;
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

  app.setTemplateTrack(*internalTrackMap.createTrack(
      CURVED, length, diameter / 2, 0, 0, 0, true, emptyUid, emptyUid, emptyUid,
      emptyUid, true));

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
    // app.setScannerStatusLabelText("Nfc tag read");

    Serial.println("registering track");

    registerScannedTracks(uid);
    app.setTargetSpeed(app.getUserSpeed());

    return;
  }

  if (app.getScannerMode() == WRITING_TRACKS) {
    // app.setScannerStatusLabelText("Writing nfc tag");

    writeTag(uid, tagJson);
    app.setTargetSpeed(app.getUserSpeed());
    return;
  }

  if (app.getScannerMode() == SCANNING_TRACKS &&
      nfcReadResults == NFC_READ_SUCCESS) {
    // app.setScannerStatusLabelText("Nfc tag read");

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
  if (app.getPreviousScannerMode() != WRITING_TRACKS) {
    app.setScannerStatusLabelText("switching back to writing");
    app.setScannerMode(WRITING_TRACKS);
    // Todo might cause error???
  }
  if (haveFirstTag) {
    app.setScannerStatusLabelText("ERROR: Scan the second tag first.");

    app.setScannerMode(app.getPreviousScannerMode());

    return;
  }

  if (internalTrackMap.getTrackCount() == 0) {
    app.setScannerStatusLabelText("ERROR: No tracks have been created.");

    app.setScannerMode(app.getPreviousScannerMode());

    return;
  }

  JsonDocument newDoc;

  JsonObject newTagJson = newDoc.to<JsonObject>();

  writeTrackForTag(uid, newTagJson);

  delay(100);

  NFCWriteResult nfcWriteResults = nfc.writeNFC(newTagJson, uid);

  if (nfcWriteResults == NFC_WRITE_SUCCESS) {
    app.setScannerStatusLabelText("Track written successfully!");

  } else if (nfcWriteResults != NFC_WRITE_DUPLICATE) {
    Serial.println("Failed to write track.");
  }
}

// ============================================================================
// ADD TRACK
// ============================================================================

void TrackScanner::addTrack(const Uid& uid, JsonObject& jsonTag) {
  if (app.getPreviousScannerMode() != SCANNING_TRACKS) {
    // Todo add a restart button for this
    app.setScannerStatusLabelText("switching back to scanning");
    internalTrackMap.clearMap();
    // Todo might cause error???
    firstTagUid.clear();
    haveFirstTag = false;
  }
  Serial.println("Scanning track");

  if (jsonTag.size() == 0) {
    app.setScannerStatusLabelText("Track scanner failed to add track");
    Serial.println("json tag empty cant make track");
    return;
  }
  Track track;
  if (!trackMap.doesTrackExist(uid)) {
    Serial.println("creating track");
    serializeJsonPretty(jsonTag, Serial);

    // Serial.println(jsonTag);

    if (track.fromCondensedJson(jsonTag)) {
      app.setScannerStatusLabelText("Adding Track");

      trackMap.addTrack(track);
    } else {
      app.setScannerStatusLabelText("Track scanner failed to add track");
      return;
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

    if (!tagConnection || !tagCurrent) {
      app.setScannerStatusLabelText("Failed To Connect Tracks");

      Serial.println("Could not find connection tag.");

      haveConnectionTag = false;
      return;
    }

    app.setScannerStatusLabelText("Adding Track and connection tags");

    trackMap.connectTags(tagConnection, tagCurrent);
    if (track.getTrackType() == STATION) {
      // stations should just be added and connected but not mess with rest of track setup
      return;
    }
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
bool TrackScanner::isDuplicate(const Uid& uid) {
  // nfc dup protection only works when past read was successful otherwise it
  // resets and fails to detect anything
  if (!havePreviousTag) {
    havePreviousTag = true;
    previousTagUid = uid;
    return false;
  }
  if (!uid.equals(previousTagUid)) {
    previousTagUid = uid;
    return false;
  }
  previousTagUid = uid;
  return true;
}
void TrackScanner::registerScannedTracks(const Uid& uid) {
  if (app.getPreviousScannerMode() != REGISTER_TRACKS) {
    // Todo add a restart button for this
    app.setScannerStatusLabelText("switching back to register");
    app.setScannerMode(REGISTER_TRACKS);
    internalTrackMap.clearMap();
    // Todo might cause error???
    firstTagUid.clear();
    haveFirstTag = false;
  }
  if (!app.getTemplateTrack()) {
    app.setScannerStatusLabelText("Error: Failed To Register Tracks");

    Serial.println("ERROR: No template track.");
    return;
  }
  // app.setScannerStatusLabelText("registerin");

  // ----------------------------------------------------------
  // First tag
  // ----------------------------------------------------------
  if (isDuplicate(uid)) {
    Serial.println("dup tag");
    // Serial.println("Scan a different tag.");
    return;
  }
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
    // app.setScannerStatusLabelText("Dup: a different tag.");

    return;
  }

  app.setScannerStatusLabelText("Second tag detected.");

  // TODO:
  // Need to have it be able to select which UID
  // exit/entrance it selects, like switches or cross. would have to add a list
  // recording unique uids seen and have user scan in certain order
  // todo have tracks cleared after registering
  // ----------------------------------------------------------
  // Reset temporary tag
  // ----------------------------------------------------------
  Track* templateTrack = app.getTemplateTrack();
  internalTrackMap.createTrack(
      templateTrack->getTrackType(), templateTrack->getLength(),
      templateTrack->getRadius(), templateTrack->getBranchAngle(),
      templateTrack->getBranchLength(), templateTrack->getBranchRadius(),
      templateTrack->getDirection(), firstTagUid, uid);

  firstTagUid.clear();
  haveFirstTag = false;

  Serial.println();
  Serial.println("Track stored.");

  Serial.print("Total tracks: ");

  Serial.println(internalTrackMap.getTrackCount());

  Serial.println();
  Serial.println("Scan the first tag of the next track.");
  app.setScannerStatusLabelText("Scan the first tag of the next track.");
}

// ============================================================================
// WRITE TRACK FOR TAG
// ============================================================================

bool TrackScanner::writeTrackForTag(const Uid& uid, JsonObject& jsonTag) {
  Track* track = internalTrackMap.findTrackByUid(uid);

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

  if (command == 'S' || command == 's') {
    Serial.println("straight Track");
    float length = 10;
    Uid emptyUid;

    app.setTemplateTrack(*internalTrackMap.createTrack(
        CURVED, length, 0, 0, 0, 0, true, emptyUid, emptyUid, emptyUid,
        emptyUid, true));

  } else if (command == 'C' || command == 'c') {
    Serial.println("curved Track");
    float diameter = 36.0;
    float length = (diameter * M_PI) / 8;
    Uid emptyUid;

    app.setTemplateTrack(*internalTrackMap.createTrack(
        CURVED, length, diameter / 2, 0, 0, 0, true, emptyUid, emptyUid,
        emptyUid, emptyUid, true));

  } else if (command == 'R' || command == 'r') {
    Serial.println("register mode");

    app.setScannerMode(REGISTER_TRACKS);

  } else if (command == 'G' || command == 'g') {
    Serial.println("scanning mode");

    app.setScannerMode(SCANNING_TRACKS);

  } else if (command == 'D' || command == 'd') {
    Serial.println("writing mode");

    app.setScannerMode(WRITING_TRACKS);
  }
}
