#pragma once
#include <Arduino.h>
#include "Config.h"
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h> 

#include "ESP_I2S.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../utils/clog.hpp"

// ============================================================
// Initialize
// ============================================================

bool AudioPlayer_begin();

// ============================================================
// Start a PCM stream
// ============================================================
//
// Returns:
//
//   0..MAX_AUDIO_STREAMS-1 = stream ID
//   -1                     = failed
//
// The PCM buffer must remain valid for the lifetime of the
// stream.
//
// ============================================================

int AudioPlayer_play(const unsigned char* samples, size_t sampleCount,
                     bool loop = false, bool initialize = true);

// ============================================================
// Stop one stream
// ============================================================

bool AudioPlayer_stop(int streamId);

bool AudioPlayer_resume(int streamId);

// ============================================================
// Stop all streams
// ============================================================

void AudioPlayer_stopAll();

// ============================================================
// Playback state
// ============================================================

bool AudioPlayer_isPlaying(int streamId);

bool AudioPlayer_isPlaying();

// ============================================================
// Speed
// ============================================================
//
// 1.0 = normal
// 0.5 = half speed
// 2.0 = double speed
//
// ============================================================

void AudioPlayer_setSpeed(int streamId, float speed);

float AudioPlayer_getSpeed(int streamId);

// ============================================================
// Pitch
// ============================================================
//
// 0    = normal
// +12  = octave up
// -12  = octave down
//
// ============================================================

void AudioPlayer_setPitch(int streamId, float semitones);

float AudioPlayer_getPitch(int streamId);

// ============================================================
// Volume
// ============================================================
//
// 0.0 = silent
// 1.0 = full volume
//
// ============================================================

void AudioPlayer_setVolume(int streamId, float volume);

float AudioPlayer_getVolume(int streamId);
