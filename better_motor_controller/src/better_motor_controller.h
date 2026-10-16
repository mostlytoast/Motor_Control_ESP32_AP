#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>

#include "Config.h"
// #include "../../shared/communication/ControllerStorage.h"
#include "../../shared/communication/EspnowController.h"
#include "../../shared/track/map.h"
#include "../../shared/utils/AppState.h"
#include "hardware/LedControl.h"
#include "hardware/MotorControl.h"
#include "hardware/NFC.h"
#include "managers/AudioManager.h"
#include "managers/WifiManager.h"
#include "track/TrackScanner.h"
#include "website/WebServer.h"

// todo include link sources of audio files?
extern char serialBuffer[SERIAL_BUFFER_SIZE];

extern size_t serialBufferLength;
