#include "SharedDisplayCalls.h"
// TODO add something that allows you to add a button that changes whenever a
// variable changes? have it so we provide it with the variable todo have to
// make sure that its updating json state and updates its received json
// SharedDisplayCalls::SharedDisplayCalls(Config config_) { config = config_; }
SharedDisplayCalls::SharedDisplayCalls() {}

void SharedDisplayCalls::actionHandler(lv_event_t* event) {
  auto* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));

  if (!binding || !binding->action || !binding->owner) {
    return;
  }

  binding->action(event);

  if (binding->owner->postActionCallback_) {
    binding->owner->postActionCallback_();
  }
}

// todo have way for this and other functions to use align as optional to
// position? or have no position

lv_obj_t* SharedDisplayCalls::createLabel(lv_obj_t* parent,
                                          lv_color_t normalColor, int x, int y,
                                          lv_part_t part, lv_align_t align,
                                          const char* text,
                                          const lv_font_t* value,
                                          lv_style_selector_t selector) {
  return createLabel(
      parent, normalColor, x, y, part, align, [text]() { return text; }, value,
      selector);
}

lv_obj_t* SharedDisplayCalls::createLabel(lv_obj_t* parent,
                                          lv_color_t normalColor, int x, int y,
                                          lv_part_t part, lv_align_t align,
                                          std::function<const char*()> getText,
                                          const lv_font_t* value,
                                          lv_style_selector_t selector) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_color(label, normalColor, part);
  lv_obj_set_style_text_font(label, value, selector);
  lv_obj_align(label, align, x, y);
  lv_label_set_text(label, getText());

  auto binding = std::make_unique<ActionBinding>(
      std::move(nullptr), nullptr, getText, this, label, normalColor,
      normalColor, normalColor);

  bindings_.push_back(std::move(binding));

  return label;
}
void SharedDisplayCalls::setLabelText(lv_obj_t* label, const char* text) {
  if (label == nullptr || text == nullptr) {
    return;
  }

  lv_label_set_text(label, text);
}
// TODO idea to add a variable callback action where you can pass in a function
// like value getVariable(bool& hasChanged - false); and if hasChanged is true
// it would update the value of the object this would have to be managed by some
// refresh function most likely that would go through a list of these functions
// and apply those changes
lv_obj_t* SharedDisplayCalls::createActionButton(
    lv_obj_t* parent, const char* text,
    std::function<void(lv_event_t* event)> action, lv_color_t normalColor,
    std::function<bool()> getState, lv_color_t activeColor,
    lv_color_t inactiveColor, lv_color_t pressedColor, lv_color_t textColor) {
  return createActionButton(parent, text, 0, 0, 0, 0, action, normalColor,
                            getState, activeColor, inactiveColor, pressedColor,
                            textColor);
}

lv_obj_t* SharedDisplayCalls::createActionButton(
    lv_obj_t* parent, const char* text, int x, int y, int width, int height,
    std::function<void(lv_event_t* event)> action, lv_color_t normalColor,
    std::function<bool()> getState, lv_color_t activeColor,
    lv_color_t inactiveColor, lv_color_t pressedColor, lv_color_t textColor) {
  lv_obj_t* button = lv_button_create(parent);

  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);

  // Button colors
  lv_obj_set_style_bg_color(button, normalColor,
                            LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_color(button, pressedColor,
                            LV_PART_MAIN | LV_STATE_PRESSED);

  // Create and store callback binding
  auto binding = std::make_unique<ActionBinding>(
      std::move(action), getState, nullptr, this, button, activeColor,
      inactiveColor, pressedColor);

  ActionBinding* bindingPtr = binding.get();

  bindings_.push_back(std::move(binding));

  lv_obj_add_event_cb(button, actionHandler, LV_EVENT_CLICKED, bindingPtr);
  lv_obj_add_event_cb(button, bindingDeleteHandler, LV_EVENT_DELETE,
                      bindingPtr);
  // Create button label
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_center(label);

  lv_obj_set_style_text_color(label, textColor, LV_PART_MAIN);

  return button;
}

lv_obj_t* SharedDisplayCalls::createActionSlider(
    lv_obj_t* parent, std::function<void(lv_event_t* event)> action,
    lv_color_t normalColor, int width, int height, int x, int y,
    std::function<bool()> getState, lv_color_t activeColor,
    lv_color_t inactiveColor, lv_color_t pressedColor) {
  lv_obj_t* slider = lv_slider_create(parent);

  lv_obj_set_pos(slider, x, y);
  lv_obj_set_size(slider, width, height);

  // slider colors
  lv_obj_set_style_bg_color(slider, normalColor,
                            LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_color(slider, pressedColor,
                            LV_PART_MAIN | LV_STATE_PRESSED);

  // Create and store callback binding
  auto binding = std::make_unique<ActionBinding>(
      std::move(action), getState, nullptr, this, slider, activeColor,
      inactiveColor, pressedColor);

  ActionBinding* bindingPtr = binding.get();

  bindings_.push_back(std::move(binding));

  lv_obj_add_event_cb(slider, actionHandler, LV_EVENT_VALUE_CHANGED,
                      bindingPtr);
  lv_obj_add_event_cb(slider, bindingDeleteHandler, LV_EVENT_DELETE,
                      bindingPtr);
  // Create slider label
  // lv_obj_t* label = lv_label_create(slider);
  // lv_label_set_text(label, text);
  // lv_obj_center(label);

  // lv_obj_set_style_text_color(label, textColor, LV_PART_MAIN);

  return slider;
}
void SharedDisplayCalls::setButtonState(lv_obj_t* button, bool state,
                                        lv_color_t activeColor,
                                        lv_color_t inactiveColor,
                                        lv_color_t pressedColor) {
  if (button == nullptr) {
    return;
  }

  lv_color_t color = state ? activeColor : inactiveColor;

  lv_obj_set_style_bg_color(button, color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(button, color, LV_PART_MAIN | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(button, pressedColor,
                            LV_PART_MAIN | LV_STATE_PRESSED);
}

lv_obj_t* SharedDisplayCalls::createActionDropdown(
    lv_obj_t* parent, const char* options, int x, int y, int width, int height,
    std::vector<std::function<void(lv_event_t* event)>> actions) {
  lv_obj_t* dropdown = lv_dropdown_create(parent);

  lv_obj_set_pos(dropdown, x, y);
  lv_obj_set_size(dropdown, width, height);

  lv_dropdown_set_options(dropdown, options);

  // Store all option actions inside one lambda.
  auto binding = std::make_unique<ActionBinding>(
      [dropdown, actions = std::move(actions)](lv_event_t* event) {
        uint16_t selected = lv_dropdown_get_selected(dropdown);

        if (selected < actions.size() && actions[selected]) {
          actions[selected](event);
        }
      },
      nullptr, nullptr, this, dropdown, internalColorConfig.activeColor,
      internalColorConfig.activeColor, internalColorConfig.activeColor);

  // auto bindinga =
  //     std::make_unique<ActionBinding>(std::move(action), getState, this,
  //     button,
  //                                     activeColor, inactiveColor,
  //                                     pressedColor);

  ActionBinding* bindingPtr = binding.get();

  bindings_.push_back(std::move(binding));

  lv_obj_add_event_cb(dropdown, actionHandler, LV_EVENT_VALUE_CHANGED,
                      bindingPtr);
  lv_obj_add_event_cb(dropdown, bindingDeleteHandler, LV_EVENT_DELETE,
                      bindingPtr);

  return dropdown;
}

lv_obj_t* SharedDisplayCalls::createTagHitbox(
    lv_obj_t* parent, int x, int y, int hitboxRadius,
    std::function<void(lv_event_t* event)> action) {
  lv_obj_t* hitbox = lv_button_create(parent);

  lv_obj_set_size(hitbox, hitboxRadius * 2, hitboxRadius * 2);

  // Position relative to root.
  lv_obj_set_pos(hitbox, static_cast<int32_t>(x - hitboxRadius),
                 static_cast<int32_t>(y - hitboxRadius));
  // Make it circular.
  lv_obj_set_style_radius(hitbox, LV_RADIUS_CIRCLE, 0);

  // Completely invisible.
  lv_obj_set_style_bg_opa(hitbox, LV_OPA_COVER, 0);
  lv_obj_move_background(hitbox);
  lv_obj_set_style_border_width(hitbox, 0, 0);
  lv_obj_set_style_outline_width(hitbox, 1, 0);

  // Enable click handling.
  lv_obj_set_clickable(hitbox, true);

  // Create and store callback binding
  auto binding = std::make_unique<ActionBinding>(
      std::move(action), nullptr, nullptr, this, hitbox,
      internalColorConfig.activeColor, internalColorConfig.activeColor,
      internalColorConfig.activeColor);

  ActionBinding* bindingPtr = binding.get();

  bindings_.push_back(std::move(binding));

  lv_obj_add_event_cb(hitbox, actionHandler, LV_EVENT_CLICKED, bindingPtr);
  lv_obj_add_event_cb(hitbox, bindingDeleteHandler, LV_EVENT_DELETE,
                      bindingPtr);
  // Create hitbox label

  return hitbox;
}
// static void ta_event_handler(lv_obj_t* ta, lv_event_t e) {
//   if (e == LV_EVENT_RELEASED) {
//     if (kb == NULL) {
//       lv_obj_set_height(cont, LV_VER_RES / 2);
//       kb = lv_keyboard_create(lv_scr_act(), NULL);
//       lv_obj_set_event_cb(kb, kb_event_cb);

//       lv_indev_wait_release(lv_indev_get_act());
//     }
//     lv_textarea_set_cursor_hidden(ta, false);
//     /*   lv_page_focus(t1, lv_textarea_get_label(ta), LV_ANIM_ON);*/
//     lv_keyboard_set_textarea(kb, ta);
//   } else if (e == LV_EVENT_DEFOCUSED) {
//     lv_textarea_set_cursor_hidden(ta, true);
//   }
// }

lv_obj_t* SharedDisplayCalls::createEditableTextBox(
    lv_obj_t* parent, lv_obj_t* keyboard,
    std::function<const char*(const char* text)> updateText, int x, int y,
    int width, int height) {
  // Todo find way to give lv_pct(100) as args
  lv_obj_t* textarea = lv_textarea_create(parent);
  lv_obj_set_align(textarea, LV_ALIGN_TOP_MID);
  lv_obj_set_pos(textarea, x, y);
  lv_obj_set_size(textarea, width, height);
  lv_textarea_set_one_line(textarea, true);

  lv_textarea_set_text(textarea, updateText(""));
  auto action = [keyboard, textarea, updateText](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
      updateText(lv_textarea_get_text(textarea));
      printf("Value: %s\n", lv_textarea_get_text(textarea));

    }

    else if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
      lv_obj_set_style_border_opa(textarea, LV_OPA_COVER,
                                  LV_PART_CURSOR | LV_STATE_FOCUSED);
      lv_obj_move_foreground(keyboard);
      lv_obj_set_hidden(keyboard, false);
      lv_keyboard_set_textarea(keyboard, textarea);
      // cursor_blink_anim_cb(textarea, false);
    } else if (lv_event_get_code(event) == LV_EVENT_DEFOCUSED) {
      updateText(lv_textarea_get_text(textarea));
      printf("Value: %s\n", lv_textarea_get_text(textarea));
      lv_obj_set_style_border_opa(textarea, LV_OPA_TRANSP,
                                  LV_PART_CURSOR | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_opa(textarea, LV_OPA_TRANSP,
                              LV_PART_CURSOR | LV_STATE_DEFAULT);

      lv_obj_set_hidden(keyboard, true);
    }
  };
  auto binding = std::make_unique<ActionBinding>(
      std::move(action), nullptr, nullptr, this, textarea,
      internalColorConfig.activeColor, internalColorConfig.activeColor,
      internalColorConfig.activeColor);

  ActionBinding* bindingPtr = binding.get();

  bindings_.push_back(std::move(binding));

  lv_obj_add_event_cb(textarea, actionHandler, LV_EVENT_CLICKED, bindingPtr);
  lv_obj_add_event_cb(textarea, actionHandler, LV_EVENT_DEFOCUSED, bindingPtr);
  lv_obj_add_event_cb(textarea, actionHandler, LV_EVENT_RELEASED, bindingPtr);
  lv_obj_add_event_cb(textarea, bindingDeleteHandler, LV_EVENT_DELETE,
                      bindingPtr);
  return textarea;
}

lv_obj_t* SharedDisplayCalls::createKeyboard(lv_obj_t* parent, const char* text,
                                             int x, int y, int width,
                                             int height) {
  // Todo could i make this compatable with web? use system keyboard
  lv_obj_t* keyboard = lv_keyboard_create(parent);
  lv_obj_set_align(keyboard, LV_ALIGN_BOTTOM_MID);
  lv_obj_set_size(keyboard, width, height);
  lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  return keyboard;
}
void SharedDisplayCalls::bindingDeleteHandler(lv_event_t* event) {
  auto* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));

  if (!binding || !binding->owner) {
    return;
  }

  auto* owner = binding->owner;

  auto it =
      std::find_if(owner->bindings_.begin(), owner->bindings_.end(),
                   [binding](const auto& ptr) { return ptr.get() == binding; });

  if (it != owner->bindings_.end()) {
    owner->bindings_.erase(it);
  }
}

void SharedDisplayCalls::updateStates() {
  for (const auto& binding : bindings_) {
    if (binding) {
      if (lv_obj_check_type(binding->object, &lv_button_class) &&
          binding->getState != nullptr) {
        setButtonState(binding->object, binding->getState(),
                       binding->activeColor, binding->inactiveColor,
                       binding->pressedColor);
      }
      if (lv_obj_check_type(binding->object, &lv_label_class) &&
          binding->getText != nullptr) {
        setLabelText(binding->object, binding->getText());
      }
    }
  }
}