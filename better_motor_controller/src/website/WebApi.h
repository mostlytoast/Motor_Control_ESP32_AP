#pragma once
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "../../shared/utils/AppState.h"
#include "../../shared/utils/StateJson.h"

#include "../Config.h"
#include "../better_motor_controller.h"
#include "../../shared/utils/MacUtils.h"
#include "../../shared/utils/StateJson.h"
#include "../../shared/communication/packets.h"
#include "../../shared/communication/espnow_controller.h"

#include "../hardware/LedControl.h"
#include "../track/TrackScanner.h"
#include "../utils/clog.hpp"
#include "WebPage.h"
#include "WebServer.h"

void registerStaticWebApp();
void registerMainPage();
void registerMotorRoutes();
void registerLedRoutes();
void registerAudioRoutes();
void registerAutoRoutes();
void registerTrackRoutes();
void registerControllerRoutes();
void registerSerialRoutes();
String getStateJSON();
void registerStateRoutes();
