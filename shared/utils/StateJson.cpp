#include "StateJson.h"

// TODO have special json funcitons that only output track when somethings
// change?
namespace {
JsonDocument previousState;
bool previousStateUpdated = false;
}  // namespace

void getStateFromJson(JsonDocument& doc) {
  Serial.println("=== getStateFromJson INPUT ===");
  serializeJsonPretty(doc, Serial);
  JsonObject state = doc["state"].as<JsonObject>();
  // TODO returning null? why
  JsonArray tracks = doc["tracks"].as<JsonArray>();

  if (!tracks.isNull()) {
    // Serial.println("convert to map ");

    trackMap.jsonToMap(tracks);
  }

  JsonObject templateTrackDoc = doc["templateTrack"].as<JsonObject>();

  if (!templateTrackDoc.isNull()) {
    bool success = false;

    Track temporaryTrack(success, templateTrackDoc, false);
    if (success) {
      temporaryTrack.setInclude(false);
      app.setTemplateTrack(temporaryTrack);
      // Serial.println("set template Track");
    } else {
      Serial.println("didn't set template Track");
    }
  }

  if (state["cur"].is<const char*>()) {
    app.setCurrentUid(Uid(state["cur"].as<const char*>()));
  }
  if (state["dest"].is<const char*>()) {
    app.setDestinationUid(Uid(state["dest"].as<const char*>()));
  }
  if (state["pf"].is<const char*>()) {
    app.setPathFinderStatusText(state["pf"].as<const char*>());
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
  if (state["aa"].is<bool>()) {
    app.setAmbianceAudioState(state["aa"].as<bool>());
  }
}

void getJsonFromState(JsonDocument& doc) {
  JsonObject state = doc["state"].to<JsonObject>();
  state["cur"] = app.getCurrentUid().toString();
  state["dest"] = app.getDestinationUid().toString();
  state["pf"] = app.getPathFinderStatusText();
  state["ssl"] = app.getScannerStatusLabelText();
  state["sl"] = app.getStatusLabelText();
  state["s"] = app.getScannerMode();
  state["1"] = app.getLed1State();
  state["2"] = app.getLed2State();
  state["a"] = app.getAutoState();
  state["p"] = app.getUserSpeed();
  state["r"] = app.getRampTime() / 1000.0f;
  state["aa"] = app.getAmbianceAudioState();

  JsonDocument mapDoc;
  trackMap.mapToJson(mapDoc);

  // mapToJson() already creates the "tracks" property.
  doc["tracks"] = mapDoc["tracks"];

  JsonObject templateTrackDoc = doc["templateTrack"].to<JsonObject>();

  if (const Track* track = app.getTemplateTrack()) {
    track->toJson(templateTrackDoc);
  }
}

void getStateFromCondensedJson(JsonDocument& doc) {
  Serial.println("=== getStateFromCondensedJson INPUT ===");
  serializeJsonPretty(doc, Serial);
  // TODO returning null? why
  JsonArray tracks = doc["tracks"].as<JsonArray>();

  if (!tracks.isNull()) {
    // Serial.println("convert to map ");

    trackMap.condensedJsonToMap(tracks);
  }

  JsonObject templateTrackDoc = doc["templateTrack"].as<JsonObject>();

  if (!templateTrackDoc.isNull()) {
    bool success = false;

    Track temporaryTrack(success, templateTrackDoc, true);
    if (success) {
      temporaryTrack.setInclude(false);
      app.setTemplateTrack(temporaryTrack);
      // Serial.println("set template Track");
    } else {
      Serial.println("didn't set template Track");
    }
  }

  JsonObject state = doc["state"].as<JsonObject>();

  if (state["cur"].is<const char*>()) {
    app.setCurrentUid(Uid(state["cur"].as<const char*>()));
  }
  if (state["dest"].is<const char*>()) {
    app.setDestinationUid(Uid(state["dest"].as<const char*>()));
  }
  if (state["pf"].is<const char*>()) {
    app.setPathFinderStatusText(state["pf"].as<const char*>());
  }
  if (state["ssl"].is<const char*>()) {
    app.setScannerStatusLabelText(state["ssl"].as<const char*>());
  }

  if (state["sl"].is<const char*>() &&
      strcmp(state["sl"].as<const char*>(), "") != 0) {
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
  if (state["aa"].is<bool>()) {
    app.setAmbianceAudioState(state["aa"].as<bool>());
  }

  JsonObject options = doc["options"].as<JsonObject>();
  if (options["mp"].is<int>() || options["mp"].is<long>() ||
      options["mp"].is<float>()) {
    app.setMinPwm(options["mp"].as<int>());
  }
  if (options["lao"].is<float>() || options["lao"].is<long>() ||
      options["lao"].is<float>()) {
    app.setLowAudioOffset(options["lao"].as<float>());
  }
  if (options["hao"].is<float>() || options["hao"].is<long>() ||
      options["hao"].is<float>()) {
    app.setHighAudioOffset(options["hao"].as<float>());
  }
  if (options["av"].is<float>() || options["av"].is<long>() ||
      options["av"].is<float>()) {
    app.setAmbianceSoundVolume(options["av"].as<float>());
  }
  if (options["mv"].is<float>() || options["mv"].is<long>() ||
      options["mv"].is<float>()) {
    app.setMovementSoundVolume(options["mv"].as<float>());
  }
}
void getCondensedJsonFromStateTransmitter(JsonDocument& doc) {
  getCondensedJsonFromState(doc);
  // doc["addTracks"] = nullptr;
  // doc["removeUid"] = nullptr;
}

void getCondensedJsonFromState(JsonDocument& doc) {
  JsonObject state = doc["state"].to<JsonObject>();
  state["cur"] = app.getCurrentUid().toString();
  state["dest"] = app.getDestinationUid().toString();
  state["pf"] = app.getPathFinderStatusText();
  state["ssl"] = app.getScannerStatusLabelText();
  state["sl"] = app.getStatusLabelText();
  state["s"] = app.getScannerMode();
  state["1"] = app.getLed1State();
  state["2"] = app.getLed2State();
  state["a"] = app.getAutoState();
  state["p"] = app.getUserSpeed();
  state["r"] = app.getRampTime() / 1000.0f;
  state["aa"] = app.getAmbianceAudioState();

  JsonDocument options;
  options["mp"] = app.getMinPwm();
  options["lao"] = app.getLowAudioOffset();
  options["hao"] = app.getHighAudioOffset();
  options["av"] = app.getAmbianceSoundVolume();
  options["mv"] = app.getMovementSoundVolume();
  doc["options"] = options;
  JsonDocument mapDoc;
  trackMap.mapToCondensedJson(mapDoc);

  // mapToJson() already creates the "tracks" property.
  doc["tracks"] = mapDoc["tracks"];

  JsonObject templateTrackDoc = doc["templateTrack"].to<JsonObject>();

  if (const Track* track = app.getTemplateTrack()) {
    track->toCondensedJson(templateTrackDoc);
  }
}

bool hasStateChanged() {
  JsonDocument state;

  getCondensedJsonFromState(state);
  if (state.isNull()) {
    return false;
  }

  if (!previousStateUpdated) {
    previousState = state;  // Ensure previousState is a JsonDocument type
    previousStateUpdated = true;
    return false;  // Return  because nothing was sent yet
  }

  if (previousState == state) {
    return false;
  }
  // Serial.println("refreshAndSend");

  previousState = state;
  return true;
}