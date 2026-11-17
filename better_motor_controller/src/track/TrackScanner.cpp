
#include "TrackScanner.h"

// TODO fix issue where if track is scanned it does not get sent to display
// unless you connect to website

// ============================================================================
// CONSTRUCTOR
// ============================================================================
Tracks internalTracks;
TrackScanner::TrackScanner(TrackMap& map)
    : trackMap(map), pathFinder(*app.getCurrentTracks()) {
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

  app.setTemplateTrack(*trackMap.createTrack(CURVED, length, diameter / 2, 0, 0,
                                             0, true, emptyUid, emptyUid,
                                             emptyUid, emptyUid, true));

  firstTagUid.clear();
  haveFirstTag = false;

  connectionTagUid.clear();
  haveConnectionTag = false;
  nfc.setupNFC();
}

// ============================================================================
// PROCESS NFC TAG
// ============================================================================

void TrackScanner::processScans() {
  app.setTargetSpeed(app.getUserSpeed());
  // if (app.getScannerMode() == NORMAL) {
  // }
  if (app.getScannerMode() == PATH_FINDING) {
    pathFind();
  }
  JsonDocument doc;
  JsonObject tagJson = doc.to<JsonObject>();

  // Temporary UID returned by the NFC reader.

  Uid uid;

  NFCReadResult nfcReadResults = nfc.readNFC(uid, tagJson);

  if (nfcReadResults == NFC_READ_FAILED || !uid.isValid()) {
    // app.setScannerStatusLabelText("Nfc Read failed");

    return;
  }

  trackLocation(uid);
  // has found a uid dont know if it got the jason tho
  // if (app.getScannerMode() != PATH_FINDING || app.getScannerMode() != NORMAL)
  // { app.setRampTime(0); app.setTargetSpeed(0);
  // }

  if (app.getScannerMode() == REGISTER_TRACKS &&
      nfcReadResults != NFC_READ_DUPLICATE) {
    // app.setScannerStatusLabelText("Nfc tag read");
    Serial.println("track type");
    Serial.println(app.getTemplateTrack()->getTrackType());

    Serial.println("registering track");

    registerScannedTracks(uid);
    // app.setTargetSpeed(app.getUserSpeed());

    return;
  }

  if (app.getScannerMode() == WRITING_TRACKS) {
    // app.setScannerStatusLabelText("Writing nfc tag");
    Serial.println("write tag");

    writeTag(uid, tagJson);
    // app.setTargetSpeed(app.getUserSpeed());
    return;
  }

  if (app.getScannerMode() == SCANNING_TRACKS &&
      nfcReadResults == NFC_READ_SUCCESS) {
    // app.setScannerStatusLabelText("Nfc tag read");
    Serial.println("scanning track");

    addTrack(uid, tagJson);
    // app.setTargetSpeed(app.getUserSpeed());

    return;
  }
}
Uid previousUid;
void TrackScanner::pathFind() {
  Uid startUid;
  if (!forwardsUid.isValid() && !backwardsUid.isValid()) {
    Serial.println("No valid uids:");

    return;
  }
  if (forwardsUid.isValid() && !backwardsUid.isValid()) {
    startUid = forwardsUid;
  } else if (backwardsUid.isValid()) {
    startUid = backwardsUid;
  }

  int setSpeed = app.getUserSpeed();

  if (!isDuplicate(startUid, previousUid)) {
    Serial.println("Finding path:");
    app.getPath().clear();
    pathFinder.findPath(startUid, app.getDestinationUid(), app.getPath());
  }
  if (!app.getPath().empty()) {
    Serial.println("Path found:");
    for (const Uid& uid : app.getPath()) {
      Serial.println(uid.toString());
    }
    String context = "Path found";
    if (forwardsUid.isValid() && app.getPath()[0].equals(forwardsUid)) {
      // go forward

      app.setTargetSpeed(setSpeed);
      context += "->";

    } else if (backwardsUid.isValid() &&
               app.getPath()[0].equals(backwardsUid)) {
      // go backwards
      context += "<-";

      app.setTargetSpeed(-setSpeed);
    }
    app.setPathFinderStatusText(context);
    // TODO find way to display found path
    for (const Uid& uid : app.getPath()) {
      Serial.println(uid.toString());
    }

  } else {
    app.setPathFinderStatusText("No path exists.");
  }
  // TODO write these functions
  // if we have a destination tag app.getDestinationUid() then we calculate
  // the path using PathFinder and use location data to decide if we should
  // move forwards or backwards
}
void TrackScanner::trackLocation(const Uid& uid) {
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
  if (haveFirstTag && app.getTemplateTrack()->getTrackType() != STATION) {
    app.setScannerStatusLabelText("ERROR: Scan the second tag first.");

    app.setScannerMode(app.getPreviousScannerMode());

    return;
  }

  if (internalTracks.size() == 0) {
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
    String successMessage = "Written";
    successMessage += uid.toString();
    app.setScannerStatusLabelText(successMessage);

  } else if (nfcWriteResults != NFC_WRITE_DUPLICATE) {
    Serial.println("Failed to write.");
  }
}

// ============================================================================
// ADD TRACK
// ============================================================================

void TrackScanner::addTrack(const Uid& uid, JsonObject& jsonTag) {
  if (app.getPreviousScannerMode() != SCANNING_TRACKS) {
    // Todo add a restart button for this
    app.setScannerStatusLabelText("switching back to scanning");
    app.setScannerMode(SCANNING_TRACKS);
    internalTracks.clear();
    // Todo might cause error???
    firstTagUid.clear();
    haveFirstTag = false;
    connectionTagUid.clear();
    haveConnectionTag = false;
  }
  Serial.println("Scanning track");

  if (jsonTag.size() == 0) {
    app.setScannerStatusLabelText("Track scanner failed to add track");
    Serial.println("json tag empty cant make track");
    return;
  }
  Track track;
  if (!app.getCurrentTracks()->doesTrackExist(uid)) {
    Serial.println("creating track");
    // serializeJsonPretty(jsonTag, Serial);

    // Serial.println(jsonTag);

    if (track.fromCondensedJson(jsonTag)) {
      app.setScannerStatusLabelText("Adding Track");

      app.getCurrentTracks()->addTrack(track);
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
    // TODO maybe add skip protection so if we have 3 tracks but middle one only
    // one connects you dont end up with 2 tracks connecting to one tag have to
    // be able to detect impossible connections and then for cases like straight
    // or curved just connect to the next tag
  } else if (app.getCurrentTracks()->doUidsBelongToSameTrack(connectionTagUid,
                                                             uid)) {
    connectionTagUid = uid;
    haveConnectionTag = true;
  } else {
    Serial.println("attempting to connect");

    // ==========================================================
    // Second connection tag
    // ==========================================================

    Tag* tagConnection = app.getCurrentTracks()->findTagByUid(connectionTagUid);

    Tag* tagCurrent = app.getCurrentTracks()->findTagByUid(uid);

    if (!tagConnection || !tagCurrent) {
      app.setScannerStatusLabelText("Failed To Connect Tracks");

      Serial.println("Could not find connection tag.");

      haveConnectionTag = false;
      return;
    }

    app.setScannerStatusLabelText("Adding Track and connection tags");

    trackMap.connectTags(tagConnection, tagCurrent);
    if (track.getTrackType() == STATION) {
      app.setScannerStatusLabelText("Adding Station");

      Serial.println("skipping clear because is station");
      // stations should just be added and connected but not mess with rest
      // of track setup
      return;
    }
    app.setScannerStatusLabelText("Adding Track and connection tags");

    connectionTagUid.clear();

    haveConnectionTag = false;
  }

  Serial.print("Total tracks: ");

  Serial.println(app.getCurrentTracks()->size());
  trackMap.calculateMap();
}

// ============================================================================
// TRACK SCANNING
// ============================================================================
bool TrackScanner::isDuplicate(const Uid& uid, Uid& previousScannedUid) {
  // nfc dup protection only works when past read was successful otherwise it
  // resets and fails to detect anything
  if (!havePreviousTag) {
    havePreviousTag = true;
    previousScannedUid = uid;
    return false;
  }
  if (!uid.equals(previousScannedUid)) {
    previousScannedUid = uid;
    return false;
  }
  previousScannedUid = uid;
  return true;
}
void TrackScanner::registerScannedTracks(const Uid& uid) {
  if (app.getPreviousScannerMode() != REGISTER_TRACKS) {
    // Todo add a restart button for this
    app.setScannerStatusLabelText("switching back to register");
    app.setScannerMode(REGISTER_TRACKS);
    internalTracks.clear();
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
  if (isDuplicate(uid, previousScannedUid)) {
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
    if (app.getTemplateTrack()->getTrackType() == STATION) {
      createTrackFromTemplate(uid);
      Serial.println();
      Serial.println("Scan the first tag of the next Station.");
      app.setScannerStatusLabelText("Scan the first tag of the next Station.");
      return;
    }

    Serial.println();
    Serial.println("Scan the second tag.");
    app.setScannerStatusLabelText("Success: scan second tag");

    return;
  }

  if (firstTagUid.equals(uid)) {
    Serial.println();
    Serial.println("Same tag detected.");
    Serial.println("Scan a different tag.");
    // app.setScannerStatusLabelText("Dup: a different tag.");

    return;
  }

  // ----------------------------------------------------------
  // Second tag
  // ----------------------------------------------------------

  app.setScannerStatusLabelText("Second tag detected.");

  // TODO:
  // Need to have it be able to select which UID
  // exit/entrance it selects, like switches or cross. would have to add a list
  // recording unique uids seen and have user scan in certain order
  // todo have tracks cleared after registering
  // ----------------------------------------------------------
  // Reset temporary tag
  // ----------------------------------------------------------
  createTrackFromTemplate(uid);

  Serial.println();
  Serial.println("Scan the first tag of the next track.");
  app.setScannerStatusLabelText("Scan the first tag of the next track.");
}

void TrackScanner::createTrackFromTemplate(const Uid& uid) {
  Uid emptyUid;
  Track* templateTrack = app.getTemplateTrack();
  Track* track = trackMap.createTrack(
      templateTrack->getTrackType(), templateTrack->getLength(),
      templateTrack->getRadius(), templateTrack->getBranchAngle(),
      templateTrack->getBranchLength(), templateTrack->getBranchRadius(),
      templateTrack->getDirection(), firstTagUid, uid, emptyUid, emptyUid,
      true);
  internalTracks.addTrack(*track);
  firstTagUid.clear();
  haveFirstTag = false;
  Serial.println();
  Serial.println("Track stored.");

  Serial.print("Total tracks: ");

  Serial.println(internalTracks.size());
}

// ============================================================================
// WRITE TRACK FOR TAG
// ============================================================================

bool TrackScanner::writeTrackForTag(const Uid& uid, JsonObject& jsonTag) {
  Track* track = internalTracks.findTrackByUid(uid);

  if (!track) {
    Serial.println();
    Serial.println("Tag was not found in the track list.");
    app.setScannerStatusLabelText("Error: can't write to unkown track");

    return false;
  }
  track->setInclude(false);

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

    app.setTemplateTrack(*trackMap.createTrack(STRAIGHT, length, 0, 0, 0, 0,
                                               true, emptyUid, emptyUid,
                                               emptyUid, emptyUid, true));

  } else if (command == 'C' || command == 'c') {
    Serial.println("curved Track");
    float diameter = 36.0;
    float length = (diameter * M_PI) / 8;
    Uid emptyUid;

    app.setTemplateTrack(*trackMap.createTrack(CURVED, length, diameter / 2, 0,
                                               0, 0, true, emptyUid, emptyUid,
                                               emptyUid, emptyUid, true));
  } else if (command == 'T' || command == 't') {
    Serial.println("station");
    Uid emptyUid;

    app.setTemplateTrack(*trackMap.createTrack(STATION, 0, 0, 0, 0, 0, true,
                                               emptyUid, emptyUid, emptyUid,
                                               emptyUid, true));

  } else if (command == 'R' || command == 'r') {
    Serial.println("register mode");

    app.setScannerMode(REGISTER_TRACKS);

  } else if (command == 'G' || command == 'g') {
    Serial.println("scanning mode");

    app.setScannerMode(SCANNING_TRACKS);

  } else if (command == 'D' || command == 'd') {
    Serial.println("writing mode");

    app.setScannerMode(WRITING_TRACKS);
  } else if (command == '+') {
    Serial.println("increase min pwm");
    MIN_PWM += 10;
    Serial.println(MIN_PWM);
  } else if (command == '-') {
    Serial.println("decrease min pwm");
    MIN_PWM -= 10;
    Serial.println(MIN_PWM);
  }
}
