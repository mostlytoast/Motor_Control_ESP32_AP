
#include "AudioManager.h"

AudioManager audio;

namespace {
float mapValue(float value, float inMin, float inMax, float outMin,
               float outMax) {
  if (inMax == inMin) {
    return outMin;
  }

  return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}
}  // namespace

AudioManager::AudioManager() {}

void AudioManager::setupAudio() {
  movementSound =
      AudioPlayer_play(__MOVEMENT_AUDIO_PCM, __MOVEMENT_AUDIO_PCM_LEN,
                       __MOVEMENT_AUDIO_PCM_LOOP, false);

  ambianceSound =
      AudioPlayer_play(__AMBIANCE_AUDIO_PCM, __AMBIANCE_AUDIO_PCM_LEN,
                       __AMBIANCE_AUDIO_PCM_LOOP, false);

  AudioPlayer_stopAll();

  AudioPlayer_setVolume(ambianceSound, ambianceSoundVolume);
  AudioPlayer_setVolume(movementSound, movementSoundVolume);
}

void AudioManager::updateAudioState() {
  updateAmbianceAudioState();
  updateMovementAudioState();
}

void AudioManager::updateAmbianceAudioState() {
  if (app.getAmbianceAudioMode()) {
    if (!AudioPlayer_isPlaying(ambianceSound)) {
      if (AudioPlayer_resume(ambianceSound)) {
        Serial.println("AMBIANCE AUDIO ON");
      } else {
        Serial.println("AMBIANCE AUDIO FAILED");
      }
    }
  } else {
    if (AudioPlayer_isPlaying(ambianceSound)) {
      AudioPlayer_stop(ambianceSound);
      Serial.println("AMBIANCE AUDIO OFF");
    }
  }
}

void AudioManager::updateMovementAudioState() {
  if (app.getMovementAudioMode()) {
    if (!AudioPlayer_isPlaying(movementSound)) {
      if (AudioPlayer_resume(movementSound)) {
        Serial.println("MOVEMENT AUDIO ON");
      } else {
        Serial.println("MOVEMENT AUDIO FAILED");
      }
    }
  } else {
    if (AudioPlayer_isPlaying(movementSound)) {
      AudioPlayer_stop(movementSound);
      Serial.println("MOVEMENT AUDIO OFF");
    }

    return;
  }

  const float speed = std::abs(static_cast<float>(app.getCurrentSpeed()));

  AudioPlayer_setSpeed(
      movementSound,
      mapValue(speed, 0.0f, 255.0f, lowAudioOffset, highAudioOffset));

  AudioPlayer_setPitch(
      movementSound,
      mapValue(speed, 0.0f, 255.0f, highAudioOffset, lowAudioOffset) / 12.0f);
}
