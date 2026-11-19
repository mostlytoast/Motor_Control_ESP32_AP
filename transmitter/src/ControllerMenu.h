#ifndef CONTROLLER_MENU_H
#define CONTROLLER_MENU_H

#include <lvgl.h>

#include <string>
#include <vector>

#include "../../shared/LvDisplay/SharedDisplayCalls.h"
#include "../../shared/communication/EspnowController.h"
#include "../../shared/utils/AppState.hpp"  // Assuming this exists
#include "Config.h"
#include "OptionsMenu.h"

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

  void refresh() ;
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
    void createMenu(lv_obj_t * screen);

    // Internal Logic
    static void scanEvent();
    void selectReceiverByMac(const uint8_t* mac);

    void selectReceiver(Peer * peer);
    void updateStatus();

    // Members
    lv_obj_t* root_;         // The main container
    lv_obj_t* list_obj;      // The scrollable list

    lv_obj_t* hide_button;
    // Internal helper to handle the fact that LVGL callbacks don't easily
    // support "this" pointers unless wrapped in a static wrapper.
    OptionsMenu* optionsMenuDisplay_;
    void handleScan(lv_event_t * event);
    SharedDisplayCalls sharedDisplay;
  };

#endif  // CONTROLLER_MENU_H
