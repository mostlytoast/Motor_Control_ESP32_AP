#include "StateJson.h"

#include "../track/map.h"
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

  if (state["scannerMode"].is<bool>()) {
    app.setScannerMode(state["scannerMode"].as<ScannerMode>());

    // Serial.print("[API] LED1: ");
    // Serial.println(state_.led1State ? "ON" : "OFF");
  }
  // ----------------------------------------------------
  // LED 1
  // ----------------------------------------------------

  if (state["led1"].is<bool>()) {
    app.setLed1State(state["led1"].as<bool>());

    // Serial.print("[API] LED1: ");
    // Serial.println(state_.led1State ? "ON" : "OFF");
  }

  // ----------------------------------------------------
  // LED 2
  // ----------------------------------------------------

  if (state["led2"].is<bool>()) {
    app.setLed2State(state["led2"].as<bool>());

    // Serial.print("[API] LED2: ");
    // Serial.println(state_.led2State ? "ON" : "OFF");
  }

  // ----------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------

  if (state["auto"].is<bool>()) {
    app.setAutoState(state["auto"].as<bool>());

    // Serial.print("[API] Auto: ");
    // Serial.println(state_.autoState ? "ON" : "OFF");
  }

  // ----------------------------------------------------
  // MOTOR SPEED
  // ----------------------------------------------------

  if (state["speed"].is<int>() || state["speed"].is<long>() ||
      state["speed"].is<float>()) {
    app.setUserSpeed(state["speed"].as<int>());

    // Serial.print("[API] Speed: ");
    // Serial.println(state_.userSpeed);
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

    // Serial.print("[API] Ramp: ");
    // Serial.print(rampSeconds);
    // Serial.println(" seconds");
  }

  // ----------------------------------------------------
  // AUDIO
  // ----------------------------------------------------

  if (state["audio"].is<bool>()) {
    app.setAudioState(state["audio"].as<bool>());

    // Serial.print("[API] Audio: ");
    // Serial.println(state_.audioState ? "ON" : "OFF");
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
  // Serial.println("=== getStateFromJson OUTPUT ===");
  // serializeJsonPretty(doc, Serial);
}

void getStateFromCondensedJson(JsonDocument& doc) {
  Serial.println("=== getStateFromCondensedJson INPUT ===");
  serializeJsonPretty(doc, Serial);
  JsonObject state = doc["state"].as<JsonObject>();
  // TODO returning null? why
  JsonArray tracks = doc["track"].as<JsonArray>();

  if (!tracks.isNull()) {
  trackMap.condensedJsonToMap(tracks);
  }

  if (state["s"].is<bool>()) {
    app.setScannerMode(state["s"].as<ScannerMode>());

    // Serial.print("[API] LED1: ");
    // Serial.println(state_.led1State ? "ON" : "OFF");
  }
  // ----------------------------------------------------
  // LED 1
  // ----------------------------------------------------

  if (state["1"].is<bool>()) {
    app.setLed1State(state["1"].as<bool>());

    // Serial.print("[API] LED1: ");
    // Serial.println(state_.led1State ? "ON" : "OFF");
  }

  // ----------------------------------------------------
  // LED 2
  // ----------------------------------------------------

  if (state["2"].is<bool>()) {
    app.setLed2State(state["2"].as<bool>());

    // Serial.print("[API] LED2: ");
    // Serial.println(state_.led2State ? "ON" : "OFF");
  }

  // ----------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------

  if (state["a"].is<bool>()) {
    app.setAutoState(state["a"].as<bool>());

    // Serial.print("[API] Auto: ");
    // Serial.println(state_.autoState ? "ON" : "OFF");
  }

  // ----------------------------------------------------
  // MOTOR SPEED
  // ----------------------------------------------------

  if (state["p"].is<int>() || state["p"].is<long>() || state["p"].is<float>()) {
    app.setUserSpeed(state["p"].as<int>());

    // Serial.print("[API] Speed: ");
    // Serial.println(state_.userSpeed);
  }

  // ----------------------------------------------------
  // RAMP
  //
  // JSON uses seconds.
  // Internal command uses milliseconds.
  // ----------------------------------------------------

  if (state["r"].is<int>() || state["r"].is<long>() || state["r"].is<float>()) {
    float rampSeconds = state["r"].as<float>();

    if (rampSeconds < 0.0f) {
      rampSeconds = 0.0f;
    }

    app.setRampTime(static_cast<int>(rampSeconds * 1000.0f));

    // Serial.print("[API] Ramp: ");
    // Serial.print(rampSeconds);
    // Serial.println(" seconds");
  }

  // ----------------------------------------------------
  // AUDIO
  // ----------------------------------------------------

  if (state["d"].is<bool>()) {
    app.setAudioState(state["d"].as<bool>());

    // Serial.print("[API] Audio: ");
    // Serial.println(state_.audioState ? "ON" : "OFF");
  }
}

void getCondensedJsonFromState(JsonDocument& doc) {
  // ----------------------------------------------------
  // STATE
  // ----------------------------------------------------

  JsonObject stateDoc = doc["state"].to<JsonObject>();
  stateDoc["s"] = app.getScannerMode();
  stateDoc["1"] = app.getLed1State();
  stateDoc["2"] = app.getLed2State();
  stateDoc["a"] = app.getAutoState();
  stateDoc["p"] = app.getUserSpeed();
  stateDoc["r"] = app.getRampTime() / 1000.0f;
  stateDoc["d"] = app.getAudioState();

  // ----------------------------------------------------
  // TRACK MAP
  // ----------------------------------------------------
  // todo maybe store trackmap in appstate?
  // TODO returning null? why

  JsonDocument mapDoc;
  
  trackMap.mapToCondensedJson(mapDoc);

  doc["track"] = mapDoc["track"];
  // Serial.println("=== getCondensedJsonFromState OUTPUT ===");
  // serializeJsonPretty(doc, Serial);
}