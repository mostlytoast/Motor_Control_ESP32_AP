#include "StateJson.h"

// TODO have special json funcitons that only output track when somethings
// change?
void getStateFromJson(JsonDocument& doc) {
  Serial.println("=== getStateFromJson INPUT ===");
  serializeJsonPretty(doc, Serial);
  JsonObject state = doc["state"].as<JsonObject>();
  JsonArray tracks = doc["tracks"].as<JsonArray>();

  if (!tracks.isNull()) {
    trackMap.JsonToMap(tracks);
  }

  JsonObject templateTrackDoc = doc["templateTrack"].as<JsonObject>();

  if (!templateTrackDoc.isNull()) {
    bool success = false;

    Track temporaryTrack(success, templateTrackDoc);

    if (success) {
      app.setTemplateTrack(temporaryTrack);
    }
  }
  if (state["scannerMode"].is<bool>()) {
    app.setScannerMode(state["scannerMode"].as<ScannerMode>());
  }
  // ----------------------------------------------------
  // LED 1
  // ----------------------------------------------------

  if (state["led1"].is<bool>()) {
    app.setLed1State(state["led1"].as<bool>());
  }

  // ----------------------------------------------------
  // LED 2
  // ----------------------------------------------------

  if (state["led2"].is<bool>()) {
    app.setLed2State(state["led2"].as<bool>());
  }

  // ----------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------

  if (state["auto"].is<bool>()) {
    app.setAutoState(state["auto"].as<bool>());
  }

  // ----------------------------------------------------
  // MOTOR SPEED
  // ----------------------------------------------------

  if (state["speed"].is<int>() || state["speed"].is<long>() ||
      state["speed"].is<float>()) {
    app.setUserSpeed(state["speed"].as<int>());
  }

  // ----------------------------------------------------
  // RAMP
  //
  // JSON uses seconds.
  // Internal command uses milliseconds.
  // ----------------------------------------------------

  if (state["ramp"].is<int>() || state["ramp"].is<long>() ||
      state["ramp"].is<float>()) {
    float rampSeconds = state["ramp"].as<float>();

    if (rampSeconds < 0.0f) {
      rampSeconds = 0.0f;
    }

    app.setRampTime(static_cast<int>(rampSeconds * 1000.0f));
  }

  // ----------------------------------------------------
  // AUDIO
  // ----------------------------------------------------

  if (state["audio"].is<bool>()) {
    app.setAudioState(state["audio"].as<bool>());
  }
}

void getJsonFromState(JsonDocument& doc) {
  // ----------------------------------------------------
  // STATE
  // ----------------------------------------------------

  JsonObject stateDoc = doc["state"].to<JsonObject>();
  stateDoc["scannerMode"] = app.getScannerMode();
  stateDoc["led1"] = app.getLed1State();
  stateDoc["led2"] = app.getLed2State();
  stateDoc["auto"] = app.getAutoState();
  stateDoc["speed"] = app.getUserSpeed();
  stateDoc["ramp"] = app.getRampTime() / 1000.0f;
  stateDoc["audio"] = app.getAudioState();

  // ----------------------------------------------------
  // TRACK MAP
  // ----------------------------------------------------
  // todo maybe store trackmap in appstate?
  JsonDocument mapDoc;
  trackMap.mapToJson(mapDoc);

  // mapToJson() already creates the "tracks" property.
  doc["tracks"] = mapDoc["tracks"];

  JsonObject templateTrackDoc = doc["templateTrack"].to<JsonObject>();

  if (const Track* track = app.getTemplateTrack()) {
    track->toJson(templateTrackDoc);
  }

  // serializeJsonPretty(doc, Serial);
}

void getStateFromCondensedJson(JsonDocument& doc) {
  Serial.println("=== getStateFromCondensedJson INPUT ===");
  serializeJsonPretty(doc, Serial);
  JsonObject state = doc["state"].as<JsonObject>();
  // TODO returning null? why
  JsonArray addTracks = doc["addTracks"].as<JsonArray>();
  JsonArray removeUid = doc["removeUid"].as<JsonArray>();

  if (!addTracks.isNull()) {
    Serial.println("convert to map ");

    trackMap.condensedJsonToMap(addTracks, removeUid);
  }

  JsonObject templateTrackDoc = doc["templateTrack"].as<JsonObject>();

  if (!templateTrackDoc.isNull()) {
    bool success = false;

    Track temporaryTrack(success, templateTrackDoc, true);

    if (success) {
      app.setTemplateTrack(temporaryTrack);
    }
  }

  if (state["ssl"].is<const char*>()) {
    app.setScannerStatusLabelText(state["ssl"].as<const char*>());
  }
  if (state["sl"].is<const char*>()) {
    // Do the same here
    app.setStatusLabelText(state["sl"].as<const char*>());
  }

  if (state["s"].is<int>()) {
    app.setScannerMode(static_cast<ScannerMode>(state["s"].as<int>()));
  }

  if (state["1"].is<bool>()) {
    app.setLed1State(state["1"].as<bool>());
  }

  if (state["2"].is<bool>()) {
    app.setLed2State(state["2"].as<bool>());
  }

  if (state["a"].is<bool>()) {
    app.setAutoState(state["a"].as<bool>());
  }

  if (state["p"].is<int>() || state["p"].is<long>() || state["p"].is<float>()) {
    app.setUserSpeed(state["p"].as<int>());
  }

  if (state["r"].is<int>() || state["r"].is<long>() || state["r"].is<float>()) {
    float rampSeconds = state["r"].as<float>();

    if (rampSeconds < 0.0f) {
      rampSeconds = 0.0f;
    }

    app.setRampTime(static_cast<int>(rampSeconds * 1000.0f));
  }

  if (state["d"].is<bool>()) {
    app.setAudioState(state["d"].as<bool>());
  }
}
void getCondensedJsonFromStateTransmitter(JsonDocument& doc) {
  getCondensedJsonFromState(doc);
  // doc["addTracks"] = nullptr;
  // doc["removeUid"] = nullptr;
}

void getCondensedJsonFromState(JsonDocument& doc) {
  JsonObject state = doc["state"].to<JsonObject>();
  state["ssl"] = app.getScannerStatusLabelText();
  state["sl"] = app.getStatusLabelText();
  state["s"] = app.getScannerMode();
  state["1"] = app.getLed1State();
  state["2"] = app.getLed2State();
  state["a"] = app.getAutoState();
  state["p"] = app.getUserSpeed();
  state["r"] = app.getRampTime() / 1000.0f;
  state["d"] = app.getAudioState();

  JsonArray addTracks = doc["addTracks"].to<JsonArray>();
  JsonArray removeUid = doc["removeUid"].to<JsonArray>();

  trackMap.mapToCondensedJson(addTracks, removeUid);
  // pastTracks = trackMap.getTracks();

  JsonObject templateTrackDoc = doc["templateTrack"].to<JsonObject>();

  if (const Track* track = app.getTemplateTrack()) {
    track->toCondensedJson(templateTrackDoc);
  }
}