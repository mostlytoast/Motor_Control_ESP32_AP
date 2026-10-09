#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <WiFi.h>

#include <esp_wifi.h>

#include "Config.h"
#include "../../shared/communication/ControllerStorage.h"
#include "../../shared/communication/espnow_controller.h"

#include "hardware/LedControl.h"
#include "hardware/MotorControl.h"
#include "hardware/NFC.h"
#include "../../shared/track/map.h"
#include "../../shared/utils/AppState.h"
#include "utils/clog.hpp"
#include "website/WebPage.h"
#include "website/WebServer.h"
// TODO use web assembly to have same ui for controller and website
// TODO find way to have shared files for esp communication (share as many files
// as possible)

// NFC nfc;
// AppState app;
// todo include link sources of audio files?
extern char serialBuffer[SERIAL_BUFFER_SIZE];

extern size_t serialBufferLength;
