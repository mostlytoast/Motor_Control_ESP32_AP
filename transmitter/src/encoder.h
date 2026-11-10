#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <stdint.h>

#include <atomic>

#include "../../shared/LvDisplay/SharedDisplayCalls.h"
#include "../../shared/LvDisplay/lvdisplay_portable.h"
#include "../../shared/SharedConfig.h"
#include "../../shared/communication/packets.h"
#include "../../shared/utils/AppState.h"
#include "../../shared/utils/MacUtils.h"
#include "../../shared/utils/StateJson.h"
#include "Config.h"
#include "ControllerMenu.h"
#include "encoder.h"
#include "lvdisplay_portable.h"
void setupEncoder();
void updateEncoder();

#endif