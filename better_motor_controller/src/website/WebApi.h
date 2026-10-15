#pragma once
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "../../shared/utils/StateJson.h"
#include "../Config.h"
#include "../better_motor_controller.h"
#include "WebServer.h"

void registerStaticWebApp();

String getStateJSON();
void registerStateRoutes();
