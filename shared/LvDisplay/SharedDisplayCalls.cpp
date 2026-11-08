#include "SharedDisplayCalls.h"
// TODO add something that allows you to add a button that changes whenever a
// variable changes? have it so we provide it with the variable todo have to
// make sure that its updating json state and updates its received json
SharedDisplayCalls::SharedDisplayCalls(
    uint32_t kColorBackground_, uint32_t kColorButtonOff_,
    uint32_t kColorButtonOn_, uint32_t kColorPressed_, uint32_t kColorText_,
    uint32_t kColorStatus_, uint32_t kColorInfo_, uint32_t KColorMenuHeader_) {
  kColorBackground = kColorBackground_;
  kColorButtonOff = kColorButtonOff_;
  kColorButtonOn = kColorButtonOn_;
  kColorPressed = kColorPressed_;
  kColorText = kColorText_;
  kColorStatus = kColorStatus_;
  kColorInfo = kColorInfo_;
  kColorMenuHeader = KColorMenuHeader_;
}

void SharedDisplayCalls::actionHandler(lv_event_t* event) {
  auto* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));

  if (!binding || !binding->action || !binding->owner) {
    return;
  }

  binding->action();

  if (binding->owner->postActionCallback_) {
    binding->owner->postActionCallback_();
  }
}
void SharedDisplayCalls::setButtonState(lv_obj_t* button, bool state) {
  if (button == nullptr) {
    return;
  }

  lv_color_t color =
      state ? lv_color_hex(kColorButtonOn) : lv_color_hex(kColorButtonOff);

  lv_obj_set_style_bg_color(button, color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(button, color, LV_PART_MAIN | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(button, lv_color_hex(kColorPressed),
                            LV_PART_MAIN | LV_STATE_PRESSED);
}
// todo have way for this and other functions to use align as optional to
// position? or have no position
lv_obj_t* SharedDisplayCalls::createLabel(lv_obj_t* parent,
                                          uint32_t normalColor, int x, int y,
                                          lv_part_t part, lv_align_t align,
                                          const char* text,
                                          const lv_font_t* value,
                                          lv_style_selector_t selector) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_color(label, lv_color_hex(normalColor), part);
  lv_obj_set_style_text_font(label, value, selector);
  lv_obj_align(label, align, x, y);
  lv_label_set_text(label, text);

  return label;
}
void SharedDisplayCalls::setLabelText(lv_obj_t* label, const char* text) {
  if (label == nullptr || text == nullptr) {
    return;
  }

  lv_label_set_text(label, text);
}
lv_obj_t* SharedDisplayCalls::createActionButton(lv_obj_t* parent,
                                                 const char* text, int x, int y,
                                                 int width, int height,
                                                 std::function<void()> action,
                                                 uint32_t normalColor) {
  lv_obj_t* button = lv_button_create(parent);

  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);

  // Button colors
  lv_obj_set_style_bg_color(button, lv_color_hex(normalColor),
                            LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_color(button, lv_color_hex(kColorPressed),
                            LV_PART_MAIN | LV_STATE_PRESSED);

  // Create and store callback binding
  auto binding = std::make_unique<ActionBinding>(std::move(action), this);

  ActionBinding* bindingPtr = binding.get();

  bindings_.push_back(std::move(binding));

  lv_obj_add_event_cb(button, actionHandler, LV_EVENT_CLICKED, bindingPtr);
  // Create button label
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_center(label);

  lv_obj_set_style_text_color(label, lv_color_hex(kColorText), LV_PART_MAIN);

  return button;
}

lv_obj_t* SharedDisplayCalls::createActionDropdown(
    lv_obj_t* parent, const char* options, int x, int y, int width, int height,
    std::vector<std::function<void()>> actions) {
  lv_obj_t* dropdown = lv_dropdown_create(parent);

  lv_obj_set_pos(dropdown, x, y);
  lv_obj_set_size(dropdown, width, height);

  lv_dropdown_set_options(dropdown, options);

  // Store all option actions inside one lambda.
  auto binding = std::make_unique<ActionBinding>(
      [dropdown, actions = std::move(actions)]() {
        uint16_t selected = lv_dropdown_get_selected(dropdown);

        if (selected < actions.size() && actions[selected]) {
          actions[selected]();
        }
      },
      this);

  ActionBinding* bindingPtr = binding.get();

  bindings_.push_back(std::move(binding));

  lv_obj_add_event_cb(dropdown, actionHandler, LV_EVENT_VALUE_CHANGED,
                      bindingPtr);

  return dropdown;
}
