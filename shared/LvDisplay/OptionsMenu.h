#ifndef OPTIONS_MENU_H
#define OPTIONS_MENU_H

#include <lvgl.h>

#include "../../shared/utils/AppState.hpp"  // Assuming this exists
#include "SharedDisplayCalls.h"

class OptionsMenu {
 public:
  using SendStateToReceiverCallback = std::function<void()>;
  OptionsMenu(lv_obj_t* parent,
              std::function<void()> sendStateToReceiverCallback_);
  void createMenu(lv_obj_t* parent);
  void refresh();
  void toggle();

 private:
  void refreshAndSend();

  SendStateToReceiverCallback sendStateToReceiverCallback = nullptr;

  SharedDisplayCalls sharedDisplay;

  lv_obj_t* root_;       // The main container
  lv_obj_t* grid_cont_;  // The main container
};
#endif  // OPTIONS_MENU_H
