#ifndef CLOG_PREFIX_DATE
#define CLOG_PREFIX_DATE (0)
#endif
#ifndef CLOG_PREFIX_PID
#define CLOG_PREFIX_PID (0)
#endif
#ifndef CLOG_PREFIX_TID
#define CLOG_PREFIX_TID (0)
#endif

#include "AudioPlayer.h"

namespace {

// ============================================================
// Multi-stream configuration
// ============================================================
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
// ============================================================

constexpr size_t MAX_AUDIO_STREAMS = 4;

// ============================================================
// Output configuration
// ============================================================

constexpr size_t AUDIO_CHUNK_SIZE = GRAIN_HOP;

// ============================================================
// I2S
// ============================================================

I2SClass i2s;

// ============================================================
// FreeRTOS task
// ============================================================

TaskHandle_t audioTaskHandle = nullptr;

// ============================================================
// Global state
// ============================================================

volatile bool audioInitialized = false;

volatile bool stopRequested = false;

// ============================================================
// Stream structure
// ============================================================

struct AudioStream {
  // ----------------------------------------------------------
  // State
  // ----------------------------------------------------------

  volatile bool active;

  volatile bool stopRequested;

  // ----------------------------------------------------------
  // PCM source
  // ----------------------------------------------------------

  const int16_t* samples;

  size_t sampleCount;

  // ----------------------------------------------------------
  // Playback
  // ----------------------------------------------------------

  float sourcePosition;

  bool loop;

  // ----------------------------------------------------------
  // Real-time controls
  // ----------------------------------------------------------

  volatile float speed;

  volatile float pitch;

  volatile float volume;
};

// ============================================================
// Stream storage
// ============================================================

AudioStream streams[MAX_AUDIO_STREAMS];

// ============================================================
// Working buffers
// ============================================================

float grainBuffer[GRAIN_SIZE];

float streamAccumulator[MAX_AUDIO_STREAMS][GRAIN_SIZE + GRAIN_HOP];

int16_t outputBuffer[AUDIO_CHUNK_SIZE];

// ============================================================
// Utility
// ============================================================

float clampFloat(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}

// ============================================================
// Pitch -> ratio
// ============================================================

float pitchToRatio(float semitones) { return powf(2.0f, semitones / 12.0f); }

// ============================================================
// Linear interpolation
// ============================================================

float interpolateSample(const int16_t* samples, size_t sampleCount,
                        float position) {
  if (samples == nullptr || sampleCount == 0) {
    return 0.0f;
  }

  if (position <= 0.0f) {
    return (float)samples[0];
  }

  if (position >= (float)(sampleCount - 1)) {
    return (float)samples[sampleCount - 1];
  }

  size_t index = (size_t)position;

  float fraction = position - (float)index;

  float a = (float)samples[index];

  float b = (float)samples[index + 1];

  return a + ((b - a) * fraction);
}

// ============================================================
// Hann window
// ============================================================

float grainWindow(size_t index) {
  if (GRAIN_SIZE <= 1) {
    return 1.0f;
  }

  return 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)index /
                             (float)(GRAIN_SIZE - 1)));
}

// ============================================================
// Float -> int16
// ============================================================

int16_t floatToPCM(float value) {
  if (value > 32767.0f) {
    value = 32767.0f;
  }

  if (value < -32768.0f) {
    value = -32768.0f;
  }

  return (int16_t)value;
}

// ============================================================
// Clear accumulators
// ============================================================

void clearAccumulators() {
  for (size_t streamIndex = 0; streamIndex < MAX_AUDIO_STREAMS; streamIndex++) {
    for (size_t i = 0; i < GRAIN_SIZE + GRAIN_HOP; i++) {
      streamAccumulator[streamIndex][i] = 0.0f;
    }
  }

  for (size_t i = 0; i < AUDIO_CHUNK_SIZE; i++) {
    outputBuffer[i] = 0;
  }
}

// ============================================================
// Initialize stream
// ============================================================

void resetStream(size_t index) {
  if (index >= MAX_AUDIO_STREAMS) {
    return;
  }

  streams[index].active = false;

  streams[index].stopRequested = false;

  streams[index].samples = nullptr;

  streams[index].sampleCount = 0;

  streams[index].sourcePosition = 0.0f;

  streams[index].loop = false;

  streams[index].speed = 1.0f;

  streams[index].pitch = 0.0f;

  streams[index].volume = 1.0f;
}

// ============================================================
// Write audio to I2S
// ============================================================

bool writeAudio(const int16_t* samples, size_t sampleCount) {
  if (samples == nullptr || sampleCount == 0) {
    return true;
  }

  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(samples);

  const size_t totalBytes = sampleCount * sizeof(int16_t);

  size_t bytesWritten = 0;

  while (bytesWritten < totalBytes && !stopRequested) {
    size_t written = i2s.write(bytes + bytesWritten, totalBytes - bytesWritten);

    if (written == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));

      continue;
    }

    bytesWritten += written;
  }

  return !stopRequested;
}

// ============================================================
// Process one stream
// ============================================================
//
// Generates one granular grain for a stream and adds it to
// that stream's accumulator.
//
// ============================================================

bool processStream(size_t streamIndex) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return false;
  }
  AudioStream& stream = streams[streamIndex];

  if (!stream.active) {
    return false;
  }

  if (stream.samples == nullptr || stream.sampleCount < 2) {
    stream.active = false;

    return false;
  }

  // ----------------------------------------------------------
  // Snapshot real-time controls
  // ----------------------------------------------------------

  float speed = stream.speed;

  float pitch = stream.pitch;

  float volume = stream.volume;

  speed = clampFloat(speed, 0.25f, 4.0f);

  pitch = clampFloat(pitch, -24.0f, 24.0f);

  volume = clampFloat(volume, 0.0f, 1.0f);

  float pitchRatio = pitchToRatio(pitch);

  // ----------------------------------------------------------
  // Check stream stop request
  // ----------------------------------------------------------

  if (stream.stopRequested) {
    stream.active = false;

    return false;
  }

  // ----------------------------------------------------------
  // Check end of source
  // ----------------------------------------------------------

  if (stream.sourcePosition >= (float)(stream.sampleCount - 1)) {
    if (stream.loop) {
      stream.sourcePosition = 0.0f;

    } else {
      stream.sourcePosition = 0.0f;
      stream.active = false;

      return false;
    }
  }

  // ==========================================================
  // Generate grain
  // ==========================================================

  for (size_t i = 0; i < GRAIN_SIZE; i++) {
    float readPosition = stream.sourcePosition + ((float)i * pitchRatio);

    // --------------------------------------------------------
    // Handle source end
    // --------------------------------------------------------

    if (readPosition >= (float)(stream.sampleCount - 1)) {
      if (stream.loop) {
        float loopLength = (float)(stream.sampleCount - 1);

        if (loopLength > 0.0f) {
          readPosition = fmodf(readPosition, loopLength);
        }

      } else {
        grainBuffer[i] = 0.0f;

        continue;
      }
    }

    // --------------------------------------------------------
    // Interpolate PCM
    // --------------------------------------------------------

    float sample =
        interpolateSample(stream.samples, stream.sampleCount, readPosition);

    // --------------------------------------------------------
    // Window
    // --------------------------------------------------------

    sample *= grainWindow(i);

    // --------------------------------------------------------
    // Volume
    // --------------------------------------------------------

    sample *= volume;

    grainBuffer[i] = sample;
  }

  // ==========================================================
  // Add grain to stream accumulator
  // ==========================================================

  for (size_t i = 0; i < GRAIN_SIZE; i++) {
    streamAccumulator[streamIndex][i] += grainBuffer[i];
  }

  // ==========================================================
  // Advance source position
  // ==========================================================
  //
  // SPEED controls this.
  //
  // PITCH does not.
  //
  // ==========================================================

  stream.sourcePosition += speed * (float)GRAIN_HOP;

  // ==========================================================
  // Loop source
  // ==========================================================

  if (stream.loop) {
    float loopLength = (float)(stream.sampleCount - 1);

    if (loopLength > 0.0f && stream.sourcePosition >= loopLength) {
      stream.sourcePosition = fmodf(stream.sourcePosition, loopLength);
    }
  }

  return true;
}

// ============================================================
// Mix streams
// ============================================================

void mixStreams() {
  // ----------------------------------------------------------
  // Clear output
  // ----------------------------------------------------------

  for (size_t i = 0; i < AUDIO_CHUNK_SIZE; i++) {
    outputBuffer[i] = 0;
  }

  // ----------------------------------------------------------
  // Mix each active stream
  // ----------------------------------------------------------

  for (size_t streamIndex = 0; streamIndex < MAX_AUDIO_STREAMS; streamIndex++) {
    if (!streams[streamIndex].active) {
      continue;
    }

    for (size_t i = 0; i < AUDIO_CHUNK_SIZE; i++) {
      float sample = streamAccumulator[streamIndex][i];

      // ----------------------------------------------------
      // Accumulate into output.
      // ----------------------------------------------------

      float mixed = (float)outputBuffer[i] + sample;

      outputBuffer[i] = floatToPCM(mixed);
    }
  }
}

// ============================================================
// Shift stream accumulators
// ============================================================

void shiftAccumulators() {
  for (size_t streamIndex = 0; streamIndex < MAX_AUDIO_STREAMS; streamIndex++) {
    for (size_t i = GRAIN_HOP; i < GRAIN_SIZE + GRAIN_HOP; i++) {
      streamAccumulator[streamIndex][i - GRAIN_HOP] =
          streamAccumulator[streamIndex][i];
    }

    for (size_t i = GRAIN_SIZE; i < GRAIN_SIZE + GRAIN_HOP; i++) {
      streamAccumulator[streamIndex][i] = 0.0f;
    }
  }
}

// ============================================================
// Check if any streams are active
// ============================================================

bool anyStreamActive() {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return false;
  }
  for (size_t i = 0; i < MAX_AUDIO_STREAMS; i++) {
    if (streams[i].active) {
      return true;
    }
  }

  return false;
}

// ============================================================
// Audio task
// ============================================================

void audioTask(void* parameter) {
  // if (!audioInitialized) {
  //   CLOGE << "AudioPlayer not initialized";

  //   return;
  // }
  (void)parameter;

  while (true) {
    // --------------------------------------------------------
    // Wait until a stream becomes active
    // --------------------------------------------------------

    if (!anyStreamActive()) {
      vTaskDelay(pdMS_TO_TICKS(5));

      continue;
    }

    stopRequested = false;

    // --------------------------------------------------------
    // Reset accumulators
    // --------------------------------------------------------

    clearAccumulators();

    // ========================================================
    // Main mixer loop
    // ========================================================

    while (!stopRequested && anyStreamActive()) {
      // ------------------------------------------------------
      // Generate one grain for every active stream.
      // ------------------------------------------------------

      for (size_t streamIndex = 0; streamIndex < MAX_AUDIO_STREAMS;
           streamIndex++) {
        processStream(streamIndex);
      }

      // ------------------------------------------------------
      // Mix all streams.
      // ------------------------------------------------------

      mixStreams();

      // ------------------------------------------------------
      // Output mixed audio.
      // ------------------------------------------------------

      if (!writeAudio(outputBuffer, AUDIO_CHUNK_SIZE)) {
        break;
      }

      // ------------------------------------------------------
      // Shift all stream accumulators.
      // ------------------------------------------------------

      shiftAccumulators();
    }

    // ========================================================
    // STOP / FINISH
    // ========================================================

    i2s.flush();

    // --------------------------------------------------------
    // Stop all streams if global stop was requested.
    // --------------------------------------------------------

    if (stopRequested) {
      for (size_t i = 0; i < MAX_AUDIO_STREAMS; i++) {
        streams[i].active = false;

        streams[i].stopRequested = false;
      }

      CLOGI << "All audio streams stopped";
    }

    clearAccumulators();

    stopRequested = false;
  }
}

}  // namespace

// ============================================================
// PUBLIC API
// ============================================================

// ============================================================
// INITIALIZE
// ============================================================

bool AudioPlayer_begin() {
  CLOGI << "Initializing AudioPlayer";

  // ----------------------------------------------------------
  // Reset stream state
  // ----------------------------------------------------------

  for (size_t i = 0; i < MAX_AUDIO_STREAMS; i++) {
    resetStream(i);
  }

  // ----------------------------------------------------------
  // Configure I2S
  // ----------------------------------------------------------

  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1, -1);

  // ----------------------------------------------------------
  // Initialize I2S
  // ----------------------------------------------------------

  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_MONO)) {
    CLOGE << "Failed to initialize I2S";

    return false;
  }

  CLOGI << "I2S initialized";

  // ----------------------------------------------------------
  // Create audio task
  // ----------------------------------------------------------

  BaseType_t result =
      xTaskCreate(audioTask, "AudioTask", AUDIO_TASK_STACK, nullptr,
                  AUDIO_TASK_PRIORITY, &audioTaskHandle);

  if (result != pdPASS) {
    CLOGE << "Failed to create audio task";

    audioTaskHandle = nullptr;

    return false;
  }

  audioInitialized = true;

  CLOGI << "AudioPlayer initialized";

  return true;
}

// ============================================================
// CREATE / START STREAM
// ============================================================
//
// Returns a stream ID.
//
// Example:
//
//   int id = AudioPlayer_play(
//     sound,
//     soundSamples,
//     false
//   );
//
// ============================================================

int AudioPlayer_play(const unsigned char* _samples, size_t sampleCount,
                     bool loop, bool initialize) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return -1;
  }

  const int16_t* samples = reinterpret_cast<const int16_t*>(_samples);

  sampleCount = sampleCount / 2;

  if (samples == nullptr || sampleCount < 2) {
    CLOGE << "Invalid PCM stream";

    return -1;
  }

  // ----------------------------------------------------------
  // Find free stream
  // ----------------------------------------------------------

  for (size_t i = 0; i < MAX_AUDIO_STREAMS; i++) {
    if (!streams[i].active) {
      AudioStream& stream = streams[i];

      stream.samples = samples;

      stream.sampleCount = sampleCount;

      stream.sourcePosition = 0.0f;

      stream.loop = loop;

      stream.speed = 1.0f;

      stream.pitch = 0.0f;

      stream.volume = 1.0f;

      stream.stopRequested = false;

      stream.active = true;

      CLOGI << "Started audio stream " << i;

      return (int)i;
    }
  }

  CLOGE << "No free audio stream slots";

  return -1;
}

// ============================================================
// STOP ONE STREAM
// ============================================================

bool AudioPlayer_stop(int streamId) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return false;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "no audio stream" << streamId;
    return false;
  }

  AudioStream& stream = streams[streamId];

  if (!stream.active) {
    CLOGI << "Audio stream already stopped" << streamId;
    return false;
  }

  stream.stopRequested = true;

  stream.active = false;

  CLOGI << "Stopped audio stream " << streamId;

  return true;
}

bool AudioPlayer_resume(int streamId) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return false;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "No audio stream" << streamId;
    return false;
  }

  AudioStream& stream = streams[streamId];

  if (stream.active) {
    CLOGI << "Audio stream already active" << streamId;

    return false;
  }

  stream.stopRequested = false;

  stream.active = true;

  CLOGI << "Resumed audio stream " << streamId;

  return true;
}

// ============================================================
// STOP ALL STREAMS
// ============================================================

void AudioPlayer_stopAll() {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return;
  }
  stopRequested = true;

  for (size_t i = 0; i < MAX_AUDIO_STREAMS; i++) {
    streams[i].stopRequested = true;

    streams[i].active = false;
  }
}

// ============================================================
// CHECK STREAM PLAYING
// ============================================================

bool AudioPlayer_isPlaying(int streamId) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return false;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "No audio stream" << streamId;
    return false;
  }

  return streams[streamId].active;
}

// ============================================================
// CHECK ANY STREAM PLAYING
// ============================================================

bool AudioPlayer_isPlaying() {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return false;
  }
  return anyStreamActive();
}

// ============================================================
// SET SPEED
// ============================================================

void AudioPlayer_setSpeed(int streamId, float speed) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "No audio stream" << streamId;
    return;
  }

  speed = clampFloat(speed, 0.25f, 4.0f);

  streams[streamId].speed = speed;
}

// ============================================================
// GET SPEED
// ============================================================

float AudioPlayer_getSpeed(int streamId) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return 0.0f;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "No audio stream" << streamId;
    return 1.0f;
  }

  return streams[streamId].speed;
}

// ============================================================
// SET PITCH
// ============================================================

void AudioPlayer_setPitch(int streamId, float semitones) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "No audio stream" << streamId;
    return;
  }

  semitones = clampFloat(semitones, -24.0f, 24.0f);

  streams[streamId].pitch = semitones;
}

// ============================================================
// GET PITCH
// ============================================================

float AudioPlayer_getPitch(int streamId) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return 0.0f;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "No audio stream" << streamId;
    return 0.0f;
  }

  return streams[streamId].pitch;
}

// ============================================================
// SET VOLUME
// ============================================================
//
// 0.0 = silent
// 1.0 = full volume
//
// ============================================================

void AudioPlayer_setVolume(int streamId, float volume) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "No audio stream" << streamId;
    return;
  }

  volume = clampFloat(volume, 0.0f, 1.0f);

  streams[streamId].volume = volume;
}

// ============================================================
// GET VOLUME
// ============================================================

float AudioPlayer_getVolume(int streamId) {
  if (!audioInitialized) {
    CLOGE << "AudioPlayer not initialized";

    return 0.0f;
  }
  if (streamId < 0 || streamId >= (int)MAX_AUDIO_STREAMS) {
    CLOGI << "No audio stream" << streamId;
    return 0.0f;
  }

  return streams[streamId].volume;
}
