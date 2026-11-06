#ifndef CONTROLLER_MENU_H
#define CONTROLLER_MENU_H

#include <lvgl.h>

#include <string>
#include <vector>

#include "../../shared/LvDisplay/SharedDisplayCalls.h"
#include "../../shared/communication/EspnowController.h"
#include "../../shared/utils/AppState.h"  // Assuming this exists
#include "Config.h"

// Forward declarations to prevent circular dependency issues
// These represent the classes in your existing project
class Peer;
// struct ReceiverEventData;

/**
 * @brief Manages the "Select Receiver" pop-up menu and logic.
 * Separates the logic of Peer selection and Scanning from the main Display
 * class.
 */
class ControllerMenu {
 public:
  /**
   * @param screen The parent object (usually the active screen) to attach the
   * menu to.
   */
  ControllerMenu(lv_obj_t* screen);
  ~ControllerMenu() = default;

  /**
   * @brief Rebuilds the list of buttons based on current discovered peers.
   * Call this whenever the list needs refreshing.
   */
  void refreshList();

  /**
   * @brief Toggles the visibility of the menu.
   */
  void toggle();

 private:
  // Internal UI Construction
  void createMenu(lv_obj_t* screen);

  // Event Handlers
  // Note: These are static or handled via lambdas to be compatible with LVGL's
  // C-style callbacks
  // static void scanEvent_Static(lv_event_t* event);
  // static void receiverSelectEvent_Static(lv_event_t* event);

  // Internal Logic
  static void scanEvent(lv_event_t* event);
  void selectReceiverByMac(const uint8_t* mac);
  // static void receiverSelectEvent(lv_event_t* event);
  void selectReceiver(Peer* peer);
  void updateStatus();

  // Members
  lv_obj_t* menu_obj;      // The main container
  lv_obj_t* list_obj;      // The scrollable list
  lv_obj_t* status_label;  // The text label for current status
  lv_obj_t* hide_button;
  // Internal helper to handle the fact that LVGL callbacks don't easily
  // support "this" pointers unless wrapped in a static wrapper.
  void handleScan(lv_event_t* event);
  SharedDisplayCalls display_;
};

#endif  // CONTROLLER_MENU_H
