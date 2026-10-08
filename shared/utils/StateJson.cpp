#include "StateJson.h"

#include "../track/map.h"

void getStateFromJson(JsonDocument& doc) {
  
  // Serial.println("=== getStateFromJson INPUT ===");
  // serializeJsonPretty(doc, Serial);
  JsonObject state = doc["state"].as<JsonObject>();
  JsonArray tracks = doc["tracks"].as<JsonArray>();

  if (!tracks.isNull()) {
    trackMap.JsonToMap(tracks);
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
  
}