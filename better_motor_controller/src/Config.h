#pragma once

#include <Arduino.h>
// change import for audio files here:
#include "audioFiles/SteamEnginePCM.h"
// constexpr char RECEIVER_NAME[] = "steam train";

// // =====================================================
// // WIFI
// // =====================================================

constexpr char AP_SSID[] = "ESP32-Access-Point";
constexpr char AP_PASSWORD[] = "12345678";

// constexpr uint8_t WIFI_CHANNEL = 6;

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
// HARDWARE
// =====================================================

constexpr int LED1_PIN = 27;
constexpr int LED2_PIN = 26;
constexpr int HALL_PIN = 35;

// MOTOR
constexpr int ENA = 13;
constexpr int IN1 = 12;
constexpr int IN2 = 14;

// NFC
constexpr int PN532_RX = 16;
constexpr int PN532_TX = 17;

// PWM
constexpr int freq = 15000;
constexpr int ledChannel = 0;
constexpr int resolution = 8;
constexpr uint16_t WEB_SERVER_PORT = 80;

constexpr size_t SERIAL_BUFFER_SIZE = 12000;

// =====================================================
// AUDIO CONFIG
// =====================================================

constexpr float lowAudioOffset = .75f;
constexpr float highAudioOffset = 5.0f;
constexpr float ambianceSoundVolume = 0.25;
constexpr float movementSoundVolume = 1.0;

// ============================================================
// I2S configuration
// ============================================================

constexpr int I2S_BCLK = 33;
constexpr int I2S_WS = 32;
constexpr int I2S_DOUT = 23;

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