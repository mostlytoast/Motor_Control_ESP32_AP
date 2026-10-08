#ifndef LVDISPLAY_PORTABLE_H
#define LVDISPLAY_PORTABLE_H

#include <lvgl.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "../communication/packets.h"
#include "../utils/StateJson.h"
#include "LvTrackMap.h"
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

struct ButtonBinding {
  LvDisplayAction action;
  LvDisplayController* controller;
};

using ButtonBindingPtr = std::unique_ptr<ButtonBinding>;

class LvDisplayController {
 public:
  explicit LvDisplayController(ILvDisplayHost* host);

  ~LvDisplayController() = default;

  void init(lv_obj_t* screen);
  void refresh();

  void setMotorSpeed(int value);
  void setRampSetting(int value);
  void setLed1(bool enabled);
  void setLed2(bool enabled);
  void setAuto(bool enabled);
  void setAudio(bool enabled);

  // const LvDisplayState& state() const { return state_; }
  lv_obj_t* speedLabel() const { return speedLabel_; }
  lv_obj_t* rampLabel() const { return rampLabel_; }
  lv_obj_t* statusLabel() const { return statusLabel_; }

  lv_obj_t* forwardButton() const { return forwardButton_; }
  lv_obj_t* stopButton() const { return stopButton_; }
  lv_obj_t* reverseButton() const { return reverseButton_; }
  lv_obj_t* rampDownButton() const { return rampDownButton_; }
  lv_obj_t* rampUpButton() const { return rampUpButton_; }
  lv_obj_t* led1Button() const { return led1Button_; }
  lv_obj_t* led2Button() const { return led2Button_; }
  lv_obj_t* autoButton() const { return autoButton_; }
  lv_obj_t* audioButton() const { return audioButton_; }

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

 private:
  void setButtonState(lv_obj_t* button, bool state);
  void updateSpeedLabel();
  void updateRampLabel();
  void updateStatusLabel();

  void buttonEventHandler(lv_event_t* event);

  lv_obj_t* createActionButton(lv_obj_t* parent, const char* text, int x, int y,
                               int width, int height, LvDisplayAction action,
                               uint32_t normalColor);

  ILvDisplayHost* host_ = nullptr;
  lv_obj_t* screen_ = nullptr;

  lv_obj_t* titleLabel_ = nullptr;
  lv_obj_t* speedLabel_ = nullptr;
  lv_obj_t* rampLabel_ = nullptr;
  lv_obj_t* statusLabel_ = nullptr;

  lv_obj_t* forwardButton_ = nullptr;
  lv_obj_t* stopButton_ = nullptr;
  lv_obj_t* reverseButton_ = nullptr;
  lv_obj_t* rampDownButton_ = nullptr;
  lv_obj_t* rampUpButton_ = nullptr;
  lv_obj_t* led1Button_ = nullptr;
  lv_obj_t* led2Button_ = nullptr;
  lv_obj_t* autoButton_ = nullptr;
  lv_obj_t* audioButton_ = nullptr;
  lv_obj_t* mapButton_ = nullptr;
  lv_obj_t* trackMapContainer_ = nullptr;
  LvTrackMap* trackMapDisplay_ = nullptr;

  bool mapVisible_ = false;
    // todo use app state instead
  std::vector<ButtonBindingPtr> bindings_;
};

#endif
