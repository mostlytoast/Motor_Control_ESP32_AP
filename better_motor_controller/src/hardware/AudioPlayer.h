#pragma once
#include <Arduino.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../../shared/utils/Math.hpp"
#include "../utils/clog.hpp"
#include "Config.h"
#include "ESP_I2S.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef CLOG_PREFIX_DATE
#define CLOG_PREFIX_DATE (0)
#endif
#ifndef CLOG_PREFIX_PID
#define CLOG_PREFIX_PID (0)
#endif
#ifndef CLOG_PREFIX_TID
#define CLOG_PREFIX_TID (0)
#endif
// TODO convert to class and include min max values for pitch volume num streams
// etc

// Initialize
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

constexpr float MIN_SPEED = 0.25f;
constexpr float MAX_SPEED = 4.0f;

constexpr float MIN_VOLUME = 0.0f;
constexpr float MAX_VOLUME = 10.0f;

constexpr float MIN_PITCH = -24.0f;
constexpr float MAX_PITCH = 24.0f;

constexpr float MIN_SEMITONES = -24.0f;
constexpr float MAX_SEMITONES = 24.0f;
  constexpr size_t MAX_AUDIO_STREAMS = 4;

  // Output configuration

  constexpr size_t AUDIO_CHUNK_SIZE = GRAIN_HOP;

struct AudioStreamData {
  // State

  volatile bool active;

  volatile bool stopRequested;

  // PCM source

  const int16_t* samples;

  size_t sampleCount;

  // Playback

  float sourcePosition;

  bool loop;

  // Real-time controls

  volatile float speed;

  volatile float pitch;

  volatile float volume;
};

class AudioPlayer {
 public:
  bool begin();

  // Start a PCM stream

  //
  // Returns:
  //
  //   0..MAX_AUDIO_STREAMS-1 = stream ID
  //   -1                     = failed
  //
  // The PCM buffer must remain valid for the lifetime of the
  // stream.
  //

  int play(const unsigned char* samples, size_t sampleCount, bool loop = false,
           bool initialize = true);

  // Stop one stream

  bool stop(int streamId);

  bool resume(int streamId);

  // Stop all streams

  void stopAll();

  // Playback state

  bool isPlaying(int streamId);

  bool isPlaying();

  // Speed

  //
  // 1.0 = normal
  // 0.5 = half speed
  // 2.0 = double speed
  //

  void setSpeed(int streamId, float speed);

  float getSpeed(int streamId);

  // Pitch

  //
  // 0    = normal
  // +12  = octave up
  // -12  = octave down
  //

  void setPitch(int streamId, float semitones);

  float getPitch(int streamId);

  // Volume

  //
  // 0.0 = silent
  // 1.0 = full volume
  //

  void setVolume(int streamId, float volume);

  float getVolume(int streamId);

 private:
  // ============================================================
  // Pitch -> ratio
  // ============================================================

  float pitchToRatio(float semitones);
  // ============================================================
  // Linear interpolation
  // ============================================================

  float interpolateSample(const int16_t* samples, size_t sampleCount,
                          float position);
  // ============================================================
  // Hann window
  // ============================================================

  float grainWindow(size_t index);
  // ============================================================
  // Float -> int16
  // ============================================================

  int16_t floatToPCM(float value);

  // ============================================================
  // Clear accumulators
  // ============================================================

  void clearAccumulators();

  // ============================================================
  // Initialize stream
  // ============================================================

  void resetStream(size_t index);
  // ============================================================
  // Write audio to I2S
  // ============================================================

  bool writeAudio(const int16_t* samples, size_t sampleCount);

  // ============================================================
  // Process one stream
  // ============================================================
  //
  // Generates one granular grain for a stream and adds it to
  // that stream's accumulator.
  //
  // ============================================================

  bool processStream(size_t streamIndex);

  // ============================================================
  // Mix streams
  // ============================================================

  void mixStreams();
  // ============================================================
  // Shift stream accumulators
  // ============================================================

  void shiftAccumulators();

  // ============================================================
  // Check if any streams are active
  // ============================================================

  bool anyStreamActive();

  // ============================================================
  // Audio task
  // ============================================================

  static void audioTask(void* parameter);

  // Multi-stream configuration

  //
  // Maximum number of simultaneous audio streams.
  //
  // Each stream consumes:
  //
  //   - state
  //   - one source position
  //
  // The source PCM itself is supplied by the caller.
  //


  // I2S

  I2SClass i2s;

  // FreeRTOS task

  TaskHandle_t audioTaskHandle = nullptr;

  // Global state

  volatile bool audioInitialized = false;

  volatile bool stopRequested = false;

  // Stream structure

  // Stream storage

  AudioStreamData streams[MAX_AUDIO_STREAMS];

  // Working buffers

  float grainBuffer[GRAIN_SIZE];

  float streamAccumulator[MAX_AUDIO_STREAMS][GRAIN_SIZE + GRAIN_HOP];

  int16_t outputBuffer[AUDIO_CHUNK_SIZE];
};