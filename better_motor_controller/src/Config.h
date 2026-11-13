#pragma once

#include "ConfigEsp32s3.h"


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

// Requested motor speed
// -255 = full reverse
//   0  = stopped
// +255 = full forward

constexpr int MIN_PWM = 140;
constexpr int MAX_PWM = 255;

// =====================================================
// CONTROLLERS
// =====================================================




// =====================================================
// AUDIO CONFIG
// =====================================================

constexpr float lowAudioOffset = .75f;
constexpr float highAudioOffset = 5.0f;
constexpr float ambianceSoundVolume = 0.25;
constexpr float movementSoundVolume = 1.0;
constexpr uint32_t SAMPLE_RATE = 24000;
// ============================================================
// Granular processing
// ============================================================
//
// 512 samples = 32 ms @ 16 kHz
//
// 256 sample hop = 16 ms
//
// ============================================================

constexpr size_t GRAIN_SIZE = 256;
constexpr size_t GRAIN_HOP = 128;

// ============================================================
// Audio task configuration
// ============================================================

constexpr uint32_t AUDIO_TASK_STACK = 8192;

constexpr UBaseType_t AUDIO_TASK_PRIORITY = 1;