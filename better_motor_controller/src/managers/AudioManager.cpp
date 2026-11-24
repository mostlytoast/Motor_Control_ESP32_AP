
#include "AudioManager.h"
AudioManager::AudioManager() {}

void AudioManager::setupAudio() {
  if (!player.begin()) {
    CLOGE << "AudioPlayer initialization failed";
  }
  movementSound = player.play(__MOVEMENT_AUDIO_PCM, __MOVEMENT_AUDIO_PCM_LEN,
                              __MOVEMENT_AUDIO_PCM_LOOP, false);

  ambianceSound = player.play(__AMBIANCE_AUDIO_PCM, __AMBIANCE_AUDIO_PCM_LEN,
                              __AMBIANCE_AUDIO_PCM_LOOP, false);

  player.stopAll();

  player.setVolume(ambianceSound, app.getAmbianceSoundVolume());
  player.setVolume(movementSound, app.getMovementSoundVolume());
}

void AudioManager::updateAudioState() {
  updateAmbianceAudioState();
  updateMovementAudioState();
}

void AudioManager::updateAmbianceAudioState() {
  player.setVolume(ambianceSound, app.getAmbianceSoundVolume());
  if (app.getAmbianceAudioState()) {
    if (!player.isPlaying(ambianceSound)) {
      if (player.resume(ambianceSound)) {
        Serial.println("AMBIANCE AUDIO ON");
      } else {
        Serial.println("AMBIANCE AUDIO FAILED");
      }
    }
  } else {
    if (player.isPlaying(ambianceSound)) {
      player.stop(ambianceSound);
      Serial.println("AMBIANCE AUDIO OFF");
    }
  }
}

void AudioManager::updateMovementAudioState() {
  player.setVolume(movementSound, app.getMovementSoundVolume());

  const float speed = std::abs((app.getCurrentSpeed()));
  if (app.getMovementAudioState() && speed != 0) {
    if (!player.isPlaying(movementSound)) {
      if (player.resume(movementSound)) {
        Serial.println("MOVEMENT AUDIO ON");
      } else {
        Serial.println("MOVEMENT AUDIO FAILED");
      }
    }
  } else {
    if (player.isPlaying(movementSound)) {
      player.stop(movementSound);
      Serial.println("MOVEMENT AUDIO OFF");
    }

    return;
  }

  player.setSpeed(movementSound,
                  mapValue(speed, MIN_PWM, MAX_PWM, app.getLowAudioOffset(),
                           app.getHighAudioOffset()));

  player.setPitch(movementSound,
                  mapValue(speed, MIN_PWM, MAX_PWM, app.getHighAudioOffset(),
                           app.getLowAudioOffset()) /
                      12.0f);
}
