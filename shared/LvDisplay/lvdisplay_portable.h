#ifndef LVDISPLAY_PORTABLE_H
#define LVDISPLAY_PORTABLE_H

#include <lvgl.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include "../SharedConfig.h"
#include "../communication/packets.h"
#include "../track/map.h"
#include "../utils/Math.hpp"
#include "../utils/StateJson.h"
#include "LvTrackMap.h"
#include "SharedDisplayCalls.h"
#include "TrackScanningDisplay.h"
// ============================================================
// Portable display dimensions
// ============================================================
#define LV_DISPLAY_PORTABLE_WIDTH 320
#define LV_DISPLAY_PORTABLE_HEIGHT 240
// ============================================================
// LVGL display actions
// ============================================================
class ILvDisplayHost {
 public:
  virtual ~ILvDisplayHost() = default;

  // virtual void sendCommand(const char* command, int value) = 0;

  virtual void sendJson(const char* json) = 0;
};
enum class LvDisplayAction {
  Forward,
  Reverse,
  Stop,
  RampUp,
  RampDown,
  Led1,
  Led2,
  Auto,
  Audio
};

// struct LvDisplayState {
//   int motorSpeed = 0;
//   int rampSetting = 0;
//   bool led1State = false;
//   bool led2State = false;
//   bool autoState = false;
//   bool audioState = false;
// };

class LvDisplayController;

class LvDisplayController {
 public:
  explicit LvDisplayController(ILvDisplayHost* host);

  ~LvDisplayController() = default;
  lv_obj_t* init(lv_obj_t* screen);

  void reset();
  void refresh();

  // const LvDisplayState& state() const { return state_; }

  void handleAction(LvDisplayAction action);

  // --------------------------------------------------------
  // JSON state synchronization
  // --------------------------------------------------------

  void getStateFromJsonDisplay(const char* json);

  // void sendStateJson();

  // --------------------------------------------------------
  // Access current state
  // --------------------------------------------------------

  // const LvDisplayState& getState() const { return state_; }
  void toggleTrackMap();
  void applyStateToUi();

  void sendStateToReceiver();

 private:
  SharedDisplayCalls sharedDisplay;


  ILvDisplayHost* host_ = nullptr;
  lv_obj_t* screen_ = nullptr;

  lv_obj_t* titleLabel_ = nullptr;
  lv_obj_t* mapButton_ = nullptr;
  lv_obj_t* trackScannerButton = nullptr;
  lv_obj_t* speedLabel_ = nullptr;
  lv_obj_t* rampLabel_ = nullptr;

  lv_obj_t* forwardButton_ = nullptr;
  lv_obj_t* stopButton_ = nullptr;
  lv_obj_t* reverseButton_ = nullptr;
  lv_obj_t* rampDownButton_ = nullptr;
  lv_obj_t* rampUpButton_ = nullptr;
  lv_obj_t* led1Button_ = nullptr;
  lv_obj_t* led2Button_ = nullptr;
  lv_obj_t* autoButton_ = nullptr;
  lv_obj_t* audioButton_ = nullptr;
  lv_obj_t* infoLabel_ = nullptr;
  lv_obj_t* grid_cont = nullptr;
  lv_obj_t* trackMapContainer_ = nullptr;
  LvTrackMap* trackMapDisplay_ = nullptr;
  lv_obj_t* trackScanningContainer_ = nullptr;
  TrackScanningDisplay* trackScanningDisplay_ = nullptr;

  // todo use app state instead
};

#endif
