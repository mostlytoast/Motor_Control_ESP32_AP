#include "WebServer.h"


AsyncWebServer server(80);


void WebServer_begin() {
  registerStaticWebApp();
  registerMainPage();
  registerTrackRoutes();
  registerControllerRoutes();
  registerSerialRoutes();
  registerStateRoutes();

  server.begin();
}
