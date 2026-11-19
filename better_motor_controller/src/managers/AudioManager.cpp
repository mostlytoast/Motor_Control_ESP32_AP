
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

  AudioPlayer_setVolume(ambianceSound, app.getAmbianceSoundVolume());
  AudioPlayer_setVolume(movementSound, app.getMovementSoundVolume());
}

void AudioManager::updateAudioState() {
  updateAmbianceAudioState();
  updateMovementAudioState();
}

void AudioManager::updateAmbianceAudioState() {
  AudioPlayer_setVolume(ambianceSound, app.getAmbianceSoundVolume());
  if (app.getAmbianceAudioState()) {
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
  AudioPlayer_setVolume(movementSound, app.getMovementSoundVolume());

  const float speed = std::abs(static_cast<float>(app.getCurrentSpeed()));

  if (app.getMovementAudioState() && speed != 0) {
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

  AudioPlayer_setSpeed(
      movementSound, mapValue(speed, MIN_PWM, MAX_PWM, app.getLowAudioOffset(),
                              app.getHighAudioOffset()));

  AudioPlayer_setPitch(
      movementSound, mapValue(speed, MIN_PWM, MAX_PWM, app.getHighAudioOffset(),
                              app.getLowAudioOffset()) /
                         12.0f);
}
