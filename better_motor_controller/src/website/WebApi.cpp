
#include "WebApi.h"
#include <espnow_controller.h>

const char* PARAM_LED = "state";
const char* PARAM_AUTO = "state";
const char* PARAM_SPEED = "value";
const char* PARAM_RAMP = "ramp";
// TODO have to unify all json into one route (mega json)
void registerStaticWebApp() {
  if (!LittleFS.begin(true)) {
    Serial.println("[web] LittleFS mount failed");
    return;
  }

  Serial.println("[web] LittleFS mounted");

  File root = LittleFS.open("/");
  File file = root.openNextFile();

  while (file) {
    Serial.printf("[web] %s (%u bytes)\n", file.name(), (unsigned)file.size());
    file = root.openNextFile();
  }

  Serial.printf("[web] index.html: %s\n",
                LittleFS.exists("/index.html") ? "YES" : "NO");

  Serial.printf("[web] index.js: %s\n",
                LittleFS.exists("/index.js") ? "YES" : "NO");

  Serial.printf("[web] index.wasm: %s\n",
                LittleFS.exists("/index.wasm") ? "YES" : "NO");

  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.js", "application/javascript");
  });

  server.on("/index.wasm", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.wasm", "application/wasm");
  });

  server.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setCacheControl("no-cache, no-store, must-revalidate");
}

void registerMainPage() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, "text/html", index_html);

    request->send(response);
  });
}

// ============================================================
// STATE
// ============================================================

String getStateJSON() {
  // notifyLocalStateChanged();
  JsonDocument outputDoc;
  getJsonFromState(outputDoc);
  String outputString;
  serializeJson(outputDoc, outputString);
  // Serial.print("new state");
  // Serial.println(outputString);
  return outputString;
}

// ============================================================
// STATE ROUTES
// ============================================================

void registerStateRoutes() {
  // ----------------------------------------------------------
  // GET /state
  //
  // Returns the current train state.
  // ----------------------------------------------------------

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest* request) {
    // notifyLocalStateChanged();
    request->send(200, "application/json", getStateJSON());
  });

  // ----------------------------------------------------------
  // POST /state
  //
  // Accepts:
  //
  // {
  //   "led1": true,
  //   "led2": false,
  //   "auto": true,
  //   "speed": 50,
  //   "ramp": 2,
  //   "audio": false
  // }
  //
  // Any field may be omitted. Only fields present in the
  // request are changed.
  // ----------------------------------------------------------

  server.on(
      "/state", HTTP_POST,

      // Request handler
      [](AsyncWebServerRequest* request) {
        // The body is processed by the body handler below.
        // Do not send a response here.
      },

      nullptr,

      // Body handler
      [](AsyncWebServerRequest* request, uint8_t* data, size_t len,
         size_t index, size_t total) {
        // ----------------------------------------------------
        // Make sure we have the complete request body.
        // ----------------------------------------------------

        if (index != 0 || len != total) {
          request->send(400, "application/json",
                        "{\"error\":\"Invalid request body\"}");
          return;
        }

        // ----------------------------------------------------
        // Parse JSON
        // ----------------------------------------------------

        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
          Serial.print("[API] Invalid JSON: ");
          Serial.println(error.c_str());

          request->send(400, "application/json",
                        "{\"error\":\"Invalid JSON\"}");

          return;
        }
        Serial.println("state change pls work");
        getStateFromJson(doc);
        // notifyLocalStateChanged();
     
        // ----------------------------------------------------
        // Return the actual state after applying the request.
        // ----------------------------------------------------

        request->send(200, "application/json", getStateJSON());
      });
}

// ============================================================
// TRACK ROUTES
// ============================================================

void registerTrackRoutes() {
  server.on("/tracks", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;

    trackMap.mapToJson(doc);

    String output;

    serializeJson(doc, output);

    request->send(200, "application/json", output);
  });
}

// ============================================================
// CONTROLLER STATE JSON
// ============================================================
// TODO move to controllers
String getControllersJSON() {
  JsonDocument doc;

  doc["pairing"] = app.getPairingMode();

  JsonArray controllers = doc["controllers"].to<JsonArray>();

  int activeController = app.getActiveController();

  if (activeController >= 0 && activeController < MAX_CONTROLLERS &&
      app.getControllers()[activeController].valid) {
    doc["active"] = app.getControllers()[activeController].name;

  } else {
    doc["active"] = "NONE";
  }

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (!app.getControllers()[i].valid) {
      continue;
    }

    JsonObject controller = controllers.add<JsonObject>();

    controller["slot"] = i;
    controller["name"] = app.getControllers()[i].name;
    controller["mac"] = macToString(app.getControllers()[i].mac);

    controller["active"] = (i == activeController);

    unsigned long age = millis() - app.getControllers()[i].lastSeen;

    controller["lastSeen"] = age;
  }

  String output;

  serializeJson(doc, output);

  return output;
}

// ============================================================
// CONTROLLER ROUTES
// ============================================================

void registerControllerRoutes() {
  // // ----------------------------------------------------------
  // // GET /controllers
  // // ----------------------------------------------------------

  // server.on("/controllers", HTTP_GET, [](AsyncWebServerRequest* request) {
  //   request->send(200, "application/json", getControllersJSON());
  // });

  // // ----------------------------------------------------------
  // // GET /pairing/toggle
  // // ----------------------------------------------------------

  // server.on("/pairing/toggle", HTTP_GET, [](AsyncWebServerRequest* request) {
  //   bool newState = !app.getPairingMode();

  //   app.setPairingMode(newState);

  //   app.setPairingModeStarted(newState ? millis() : 0);

  //   Serial.print("Pairing mode: ");
  //   Serial.println(newState ? "OPEN" : "CLOSED");

  //   request->send(200, "text/plain", newState ? "OPEN" : "CLOSED");
  // });

  // // ----------------------------------------------------------
  // // GET /controller/remove?slot=N
  // // ----------------------------------------------------------

  // server.on("/controller/remove", HTTP_GET, [](AsyncWebServerRequest* request) {
  //   if (!request->hasParam("slot")) {
  //     request->send(400, "text/plain", "Missing slot");

  //     return;
  //   }

  //   int slot = request->getParam("slot")->value().toInt();

  //   if (removeController(slot)) {
  //     request->send(200, "text/plain", "REMOVED");

  //   } else {
  //     request->send(404, "text/plain", "NOT FOUND");
  //   }
  // });
}

// ============================================================
// SERIAL ROUTES
// ============================================================

void registerSerialRoutes() {
  // ----------------------------------------------------------
  // GET /serial
  // ----------------------------------------------------------

  server.on("/serial", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", serialBuffer);
  });

  // ----------------------------------------------------------
  // GET /serial/clear
  // ----------------------------------------------------------

  server.on("/serial/clear", HTTP_GET, [](AsyncWebServerRequest* request) {
    serialBufferLength = 0;
    serialBuffer[0] = '\0';

    request->send(200, "text/plain", "OK");
  });
}