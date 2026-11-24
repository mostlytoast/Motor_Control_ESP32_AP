#pragma once

#if defined(BOARD_DEV)

#include "ConfigEsp32.h"

#elif defined(BOARD_S3)

#include "ConfigEsp32s3.h"

#else

#error "No board configuration selected! Define BOARD_A or BOARD_B."

#endif

// change import for audio files here:
#include "audioFiles/SteamEnginePCM.h"

// =====================================================
// WIFI
// =====================================================

constexpr char AP_SSID[] = "ESP32-Access-Point";
constexpr char AP_PASSWORD[] = "12345678";

// =====================================================
// Motor control variables
// =====================================================


// =====================================================
// CONTROLLERS
// =====================================================

// =====================================================
// AUDIO CONFIG
// =====================================================

// constexpr float lowAudioOffset = .75f;
// constexpr float highAudioOffset = 5.0f;
constexpr float ambianceSoundVolume = 0.25;
constexpr float movementSoundVolume = 1.0;
