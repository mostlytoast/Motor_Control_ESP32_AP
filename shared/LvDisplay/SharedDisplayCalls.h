#ifndef SHARED_DISPLAY_CALLS_H
#define SHARED_DISPLAY_CALLS_H

#include <lvgl.h>
// #include <src/widgets/keyboard/lv_keyboard.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// TODO have uniform call structure
// int width = 0, int height = 0, int x = 0, int y = 0,
class SharedDisplayCalls;
// Non-templated button binding
struct ActionBinding {
  std::function<void(lv_event_t* event)> action;
  std::function<bool()> getState;
  std::function<const char*()> getText;
  // std::function<bool()> getText;
  // std::function<bool()> getOptions;
  SharedDisplayCalls* owner;
  lv_obj_t* object;
  lv_color_t activeColor;
  lv_color_t inactiveColor;
  lv_color_t pressedColor;
  ActionBinding(std::function<void(lv_event_t* event)> action_,
                std::function<bool()> getState_,
                std::function<const char*()> getText_,
                SharedDisplayCalls* owner_, lv_obj_t* object_,
                lv_color_t activeColor_, lv_color_t inactiveColor_,
                lv_color_t pressedColor_)
      : action(std::move(action_)),
        getState(std::move(getState_)),
        getText(std::move(getText_)),
        owner(owner_),
        object(object_),
        activeColor(activeColor_),
        inactiveColor(inactiveColor_),
        pressedColor(pressedColor_) {}
};

using ActionBindingPtr = std::unique_ptr<ActionBinding>;

struct ActionBindingSlider {
  std::function<void(float value)> action;
  std::function<float()> getState;
  SharedDisplayCalls* owner;
  lv_obj_t* object;
  ActionBindingSlider(std::function<void(float value)> action_,
                      std::function<float()> getState_,
                      SharedDisplayCalls* owner_, lv_obj_t* object_)
      : action(std::move(action_)),
        getState(std::move(getState_)),
        owner(owner_),
        object(object_) {}
};

using ActionBindingSliderPtr = std::unique_ptr<ActionBindingSlider>;
struct InternalColorConfig {
  lv_color_t backgroundColor = lv_color_hex(0x101820);
  lv_color_t labelColor = lv_color_hex(0x003249);
  lv_color_t textColor = lv_color_hex(0xFFFFFF);
  lv_color_t inactiveColor = lv_color_hex(0x444444);
  lv_color_t activeColor = lv_color_hex(0x00AA55);
  lv_color_t pressedColor = lv_color_hex(0x222222);
  lv_color_t statusColor = lv_color_hex(0x00FFFF);
  lv_color_t infoColor = lv_color_hex(0x888888);
  lv_color_t forwardColor = lv_color_hex(0x0077CC);
  lv_color_t reverseColor = lv_color_hex(0x0077CC);
  lv_color_t stopColor = lv_color_hex(0xCC2222);
  lv_color_t menuHeaderColor = lv_color_hex(0x005577);
};

class SharedDisplayCalls {
 public:
  //  todo need to be able to call this and provide my own color config
  SharedDisplayCalls();

  ~SharedDisplayCalls() = default;

  void setPostActionCallback(std::function<void()> callback_) {
    postActionCallback_ = std::move(callback_);
  }

  lv_obj_t* createLabel(lv_obj_t* parent, lv_color_t normalColor, int x = 0,
                        int y = 0, lv_part_t part = LV_PART_MAIN,
                        lv_align_t align = LV_ALIGN_CENTER,
                        const char* text = "",
                        const lv_font_t* value = &lv_font_montserrat_16,
                        lv_style_selector_t selector = LV_PART_MAIN);
  lv_obj_t* createLabel(lv_obj_t* parent, lv_color_t normalColor, int x = 0,
                        int y = 0, lv_part_t part = LV_PART_MAIN,
                        lv_align_t align = LV_ALIGN_CENTER,
                        std::function<const char*()> getText = nullptr,
                        const lv_font_t* value = &lv_font_montserrat_16,
                        lv_style_selector_t selector = LV_PART_MAIN);

  void setLabelText(lv_obj_t* label, const char* text);

  lv_obj_t* createActionButton(
      lv_obj_t* parent, const char* text,
      std::function<void(lv_event_t* event)> action, lv_color_t normalColor,
      std::function<bool()> getState = nullptr,
      lv_color_t activeColor = lv_color_hex(0x00AA55),
      lv_color_t inactiveColor = lv_color_hex(0x444444),
      lv_color_t pressedColor = lv_color_hex(0x222222),
      lv_color_t textColor = lv_color_hex(0xFFFFFF));

  // Create a button with any callable action
  lv_obj_t* createActionButton(
      lv_obj_t* parent, const char* text, int x, int y, int width, int height,
      std::function<void(lv_event_t* event)> action, lv_color_t normalColor,
      std::function<bool()> getState = nullptr,
      lv_color_t activeColor = lv_color_hex(0x00AA55),
      lv_color_t inactiveColor = lv_color_hex(0x444444),
      lv_color_t pressedColor = lv_color_hex(0x222222),
      lv_color_t textColor = lv_color_hex(0xFFFFFF));

  void setButtonState(lv_obj_t* button, bool state, lv_color_t activeColor,
                      lv_color_t inactiveColor, lv_color_t pressedColor);
  struct DropdownItem {
    const char* text;
    std::function<void(lv_event_t* event)> action;
  };

  lv_obj_t* createActionSlider(
      lv_obj_t* parent, std::function<void(float value)> action,
      std::function<float()> getState, float min, float max,
      lv_color_t normalColor, int width = 0, int height = 0, int x = 0,
      int y = 0, lv_color_t activeColor = lv_color_hex(0x00AA55),
      lv_color_t inactiveColor = lv_color_hex(0x444444),
      lv_color_t pressedColor = lv_color_hex(0x222222));

  lv_obj_t* createActionDropdown(
      lv_obj_t* parent, const char* options, int x, int y, int width,
      int height, std::vector<std::function<void(lv_event_t* event)>> actions);

  lv_obj_t* createTagHitbox(lv_obj_t* parent, int x, int y, int hitboxRadius,
                            std::function<void(lv_event_t* event)> action,
                            std::function<bool()> getState = nullptr,
                            lv_color_t activeColor = lv_color_hex(0x00AA55),
                            lv_color_t inactiveColor = lv_color_hex(0x444444),
                            lv_color_t pressedColor = lv_color_hex(0x222222));
  lv_obj_t* createEditableTextBox(
      lv_obj_t* parent, lv_obj_t* keyboard,
      std::function<const char*(const char* text)> updateText, int x = 0,
      int y = 0, int width = 0, int height = 0);
  lv_obj_t* createKeyboard(lv_obj_t* parent, const char* text, int x, int y,
                           int width, int height);

  static void bindingDeleteHandler(lv_event_t* event);
  void updateStates();

 private:
  std::vector<ActionBindingPtr> bindings_;
  std::vector<ActionBindingSliderPtr> bindingsSlider_;
  std::function<void()> postActionCallback_;
  static void actionHandler(lv_event_t* event);
  static void actionHandlerSlider(lv_event_t* event);

  InternalColorConfig internalColorConfig;
};

#endif  // SHARED_DISPLAY_CALLS_H
