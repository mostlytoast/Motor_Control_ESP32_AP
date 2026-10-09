#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "WebApi.h"
extern AsyncWebServer server;

void WebServer_begin();
