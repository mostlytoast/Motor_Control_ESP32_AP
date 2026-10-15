#include "WebServer.h"


AsyncWebServer server(80);


void WebServer_begin() {
  registerStaticWebApp();
  registerStateRoutes();
  server.begin();
  Serial.println("Webserver started!");
}
