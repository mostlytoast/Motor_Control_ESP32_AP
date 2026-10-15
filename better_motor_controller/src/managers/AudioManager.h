#pragma once

#include <Arduino.h>

#include <cmath>

#include "../../../shared/utils/AppState.h"
#include "../Config.h"
#include "../hardware/AudioPlayer.h"

class AudioManager {
 public:
  AudioManager();

  void setupAudio();
  void updateAudioState();

 private:
  int movementSound = 0;
  int ambianceSound = 0;
  int sensorValue = 0;

  void updateAmbianceAudioState();
  void updateMovementAudioState();
};

extern AudioManager audio;
