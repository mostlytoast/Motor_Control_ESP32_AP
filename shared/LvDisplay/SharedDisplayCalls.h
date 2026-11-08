#ifndef SHARED_DISPLAY_CALLS_H
#define SHARED_DISPLAY_CALLS_H

#include <lvgl.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class SharedDisplayCalls;
// Non-templated button binding
struct ActionBinding {
  std::function<void()> action;
  SharedDisplayCalls* owner;

  ActionBinding(std::function<void()> action_, SharedDisplayCalls* owner_)
      : action(std::move(action_)), owner(owner_) {}
};
using ActionBindingPtr = std::unique_ptr<ActionBinding>;

class SharedDisplayCalls {
 public:
  SharedDisplayCalls(uint32_t kColorBackground = 0x101820,
                     uint32_t kColorButtonOff = 0x444444,
                     uint32_t kColorButtonOn = 0x00AA55,
                     uint32_t kColorPressed = 0x222222,
                     uint32_t kColorText = 0xFFFFFF,
                     uint32_t kColorStatus = 0x00FFFF,
                     uint32_t kColorInfo = 0x888888,
                     // uint32_t kColorForward = 0x0077CC,
                     // uint32_t kColorReverse = 0x0077CC,
                     //  uint32_t kColorStop = 0xCC2222,
                     uint32_t kColorMenuHeader = 0x005577);

  ~SharedDisplayCalls() = default;

  void setPostActionCallback(std::function<void()> callback_) {
    postActionCallback_ = std::move(callback_);
  }

  lv_obj_t* createLabel(lv_obj_t* parent, uint32_t normalColor, int x, int y,
                        lv_part_t part, lv_align_t align, const char *text="",
                        const lv_font_t* value = &lv_font_montserrat_16,
                        lv_style_selector_t selector = LV_PART_MAIN);
  void setLabelText(lv_obj_t* label, const char* text);
  // Create a button with any callable action
  lv_obj_t* createActionButton(lv_obj_t* parent, const char* text, int x, int y,
                               int width, int height,
                               std::function<void()> action,
                               uint32_t normalColor);

  void setButtonState(lv_obj_t* button, bool state);
  struct DropdownItem {
    const char* text;
    std::function<void()> action;
  };

  lv_obj_t* createActionDropdown(lv_obj_t* parent, const char* options, int x,
                                 int y, int width, int height,
                                 std::vector<std::function<void()>> actions);

 private:
  std::vector<ActionBindingPtr> bindings_;
  std::function<void()> postActionCallback_;
  static void actionHandler(lv_event_t* event);

  uint32_t kColorBackground = 0x101820;
  uint32_t kColorButtonOff = 0x444444;
  uint32_t kColorButtonOn = 0x00AA55;
  uint32_t kColorPressed = 0x222222;
  uint32_t kColorText = 0xFFFFFF;
  uint32_t kColorStatus = 0x00FFFF;
  uint32_t kColorInfo = 0x888888;
  // uint32_t kColorForward = 0x0077CC;
  // uint32_t kColorReverse = 0x0077CC;
  // uint32_t kColorStop = 0xCC2222;
  uint32_t kColorMenuHeader = 0x005577;
};

#endif  // SHARED_DISPLAY_CALLS_H