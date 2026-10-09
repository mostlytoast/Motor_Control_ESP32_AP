
#include "TrackScanner.h"

// TODO fix issue where if track is scanned it does not get sent to display unless you connect to website

// ============================================================================
// CONSTRUCTOR
// ============================================================================

TrackScanner::TrackScanner(TrackMap& map) : trackMap(map) {
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
                                            0, false, emptyUid, emptyUid,
                                            emptyUid, emptyUid, true));
  app.setScannerMode(REGISTER_TRACKS);

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
}

// ============================================================================
// PROCESS NFC TAG
// ============================================================================

void TrackScanner::processScans() {
  Track* templateTrack = app.getTemplateTrack();

  JsonDocument doc;
  JsonObject tagJson = doc.to<JsonObject>();

  // Temporary UID returned by the NFC reader.
  Uid uid;

  NFCReadResult nfcReadResults = nfc.readNFC(uid, tagJson);

  if (nfcReadResults == NFC_READ_FAILED) {
    return;
  }

  if (!uid.isValid()) {
    Serial.println("Invalid UID.");
    return;
  }

  if (app.getScannerMode() == REGISTER_TRACKS &&
      nfcReadResults != NFC_READ_DUPLICATE) {
    Serial.println("registering track");

    registerScannedTracks(uid, templateTrack);

    return;
  }

  if (app.getScannerMode() == WRITING_TRACKS) {
    writeTag(uid, tagJson);

    return;
  }

  if (app.getScannerMode() == SCANNING_TRACKS &&
      nfcReadResults == NFC_READ_SUCCESS) {
    addTrack(uid, tagJson);

    return;
  }
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
    Serial.println();
    Serial.println("ERROR: No tracks have been created.");

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

    Track* track = trackMap.jsonToTrack(jsonTag);

    if (!track) {
      Serial.println("Failed to create track from JSON.");
      return;
    }

    //   Serial.printf(
    //       "Track : positioned=%d "
    //       "entrance=(%.2f, %.2f) "
    //       "exit=(%.2f, %.2f)\n",
    //       track->positioned, track->pose.entrance.x, track->pose.entrance.y,
    //       track->pose.exit.x, track->pose.exit.y);
  }

  // ============================================================
  // First connection tag
  // ============================================================

  if (!haveConnectionTag) {
    Serial.println("saving connection tag");

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
      Serial.println("Could not find connection tag.");

      haveConnectionTag = false;
      return;
    }

    if (!tagCurrent) {
      Serial.println("Could not find current tag.");

      haveConnectionTag = false;
      return;
    }

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

void TrackScanner::registerScannedTracks(const Uid& uid, Track* templateTrack) {
  if (!templateTrack) {
    Serial.println("ERROR: No template track.");
    return;
  }

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

    return;
  }

  // ----------------------------------------------------------
  // Second tag
  // ----------------------------------------------------------

  if (firstTagUid.equals(uid)) {
    Serial.println();
    Serial.println("Same tag detected.");
    Serial.println("Scan a different tag.");

    return;
  }

  Serial.println();
  Serial.println("Second tag detected.");

  // TODO:
  // Need to have it be able to select which UID
  // exit/entrance it selects, like switches or cross. would have to add a list
  // recording unique uids seen and have user scan in certain order

  Uid emptyUid;

  Track* track = trackMap.createTrack(
      templateTrack->trackType, templateTrack->length, templateTrack->radius,
      templateTrack->branchAngle, templateTrack->branchRadius,
      templateTrack->branchLength, templateTrack->direction,

      firstTagUid, uid,

      emptyUid, emptyUid);
  // todo have tracks cleared after registering
  if (!track) {
    Serial.println("ERROR: Failed to create track.");

    return;
  }

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
}

// ============================================================================
// WRITE TRACK FOR TAG
// ============================================================================

bool TrackScanner::writeTrackForTag(const Uid& uid, JsonObject& jsonTag) {
  Track* track = trackMap.findTrackByUid(uid);

  if (!track) {
    Serial.println();
    Serial.println("Tag was not found in the track list.");

    return false;
  }

  trackMap.trackToJson(*track, jsonTag);

  // Serial.println("========================================");

  // Serial.println("TRACK FOUND");

  // Serial.println("========================================");

  // Serial.println();

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
    Serial.println("writting mode");

    app.setScannerMode(WRITING_TRACKS);
  }
}
