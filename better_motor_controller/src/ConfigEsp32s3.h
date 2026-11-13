#pragma once

#include <Arduino.h>
// change import for audio files here:


// =====================================================
// HARDWARE
// =====================================================

// MOTOR
constexpr int ENA = 14;
constexpr int IN1 = 13;
constexpr int IN2 = 12;
// LED
constexpr int LED1_PIN = 11;
constexpr int LED2_PIN = 10;


// NFC
constexpr int PN532_RX = 18;
constexpr int PN532_TX = 17;

// PWM
constexpr int freq = 15000;
constexpr int ledChannel = 0;
constexpr int resolution = 8;
constexpr uint16_t WEB_SERVER_PORT = 80;

constexpr size_t SERIAL_BUFFER_SIZE = 12000;


// ============================================================
// I2S configuration
// ============================================================

constexpr int I2S_BCLK = 6;
constexpr int I2S_WS = 16;
constexpr int I2S_DOUT = 42;




