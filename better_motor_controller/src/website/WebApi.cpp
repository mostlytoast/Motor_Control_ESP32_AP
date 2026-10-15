
#include "WebApi.h"
// ============================================================
// STATE ROUTES
// ============================================================

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
        notifyLocalStateChanged();
     
        // ----------------------------------------------------
        // Return the actual state after applying the request.
        // ----------------------------------------------------

        request->send(200, "application/json", getStateJSON());
      });
}

// ============================================================
// STATE
// ============================================================

String getStateJSON() {
  notifyLocalStateChanged();
  JsonDocument outputDoc;
  getJsonFromState(outputDoc);
  String outputString;
  serializeJson(outputDoc, outputString);
  // Serial.print("new state");
  // Serial.println(outputString);
  return outputString;
}


