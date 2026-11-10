// ControllerMenu.h / .cpp
#include "ControllerMenu.h"

// struct ReceiverEventData {
//   Display* display;  // Keep reference to the main display if needed
//   uint8_t mac[6];
// };

ControllerMenu::ControllerMenu(lv_obj_t* screen) { createMenu(screen); }
void ControllerMenu::createMenu(lv_obj_t* screen) {
  menu_obj = lv_obj_create(screen);
  lv_obj_set_size(menu_obj, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(menu_obj, 0, 0);
  // lv_obj_set_scrollbar_mode(list_obj, LV_STATE_DISABLED);

  // lv_obj_set_scroll_edge(menu_obj, LV_SCROLL_EDGE_NONE);

  lv_obj_set_style_bg_color(menu_obj, lv_color_hex(COLOR_MENU), LV_PART_MAIN);
  lv_obj_set_style_border_color(menu_obj, lv_color_hex(COLOR_STATUS),
                                LV_PART_MAIN);
  lv_obj_set_style_border_width(menu_obj, 2, LV_PART_MAIN);

  // Header
  lv_obj_t* header = lv_label_create(menu_obj);
  lv_label_set_text(header, "SELECT RECEIVER");
  lv_obj_set_style_text_color(header, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 5);

  // List
  // List
  // List
  list_obj = lv_obj_create(menu_obj);
  // Increase the height (250) to ensure there is room for multiple buttons
  lv_obj_set_size(list_obj, 290, 250);
  lv_obj_align(list_obj, LV_ALIGN_TOP_MID, 0, 32);
  lv_obj_set_style_bg_opa(list_obj, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(list_obj, 0, LV_PART_MAIN);

  // --- Fixed Flex Layout for your specific LVGL version ---
  lv_obj_set_flex_flow(list_obj, LV_FLEX_FLOW_COLUMN);

  // The compiler told us this needs 3 arguments:
  lv_obj_set_flex_align(list_obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  // The compiler told us to use this instead of 'optical'
  lv_obj_set_flex_grow(list_obj, 0);

  // This replaces the 'scrollbar_status' error

  lv_obj_set_style_pad_row(list_obj, 5, LV_PART_MAIN);

  espnowController.setReceiverListNeedsRefresh(true);

  hide_button = display_.createActionButton(
      menu_obj, "Hide", 215, 27, 100, 25, [this]() { toggle(); },
      COLOR_MENU_HEADER);
  lv_obj_set_hidden(menu_obj, true);
}

void ControllerMenu::refreshList() {
  lv_obj_clean(list_obj);

  // Scan button
  display_.createActionButton(
      list_obj, "SCAN / DISCOVER", 280, 36, 100, 40,
      [this]() { this->scanEvent(nullptr); }, COLOR_SCAN);

  // Receiver buttons
  for (const Peer& peer : espnowController.getPeers()) {
    // Copy MAC address while the peer is valid.
    uint8_t mac[6];
    memcpy(mac, peer.getMac(), sizeof(mac));

    lv_obj_t* button = display_.createActionButton(
        list_obj, "", 280, 36, 100, 40,
        [this, mac]() { this->selectReceiverByMac(mac); }, COLOR_SCAN);

    bool selected = peer.getState() == PeerState::ACTIVE;

    lv_obj_set_style_bg_color(button,
                              selected ? lv_color_hex(COLOR_SELECTED)
                                       : lv_color_hex(COLOR_BUTTON_OFF),
                              LV_PART_MAIN);

    char text[64];

    snprintf(text, sizeof(text), "%s%s", selected ? "> " : "", peer.getName());

    lv_obj_t* label = lv_label_create(button);

    lv_label_set_text(label, text);

    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);

    lv_obj_center(label);
  }

  if (espnowController.getNumberOfReceivers() == 0) {
    lv_obj_t* none = lv_label_create(list_obj);

    lv_label_set_text(none, "No receivers found");

    lv_obj_set_style_text_color(none, lv_color_hex(COLOR_INFO), LV_PART_MAIN);
  }
}

// Logic methods
void ControllerMenu::scanEvent(lv_event_t* event) {
  espnowController.resetDiscover();

  app.setStatusLabelText("No Peer");

  Serial.println("Starting receiver discovery...");
}

void ControllerMenu::selectReceiverByMac(const uint8_t* mac) {
  if (mac == nullptr) {
    Serial.println("ERROR: Null receiver MAC");
    return;
  }

  Serial.print("Selecting receiver: ");
  Serial.println(macToString(mac));

  Peer* peer = peers.findPeer(mac);

  if (peer == nullptr) {
    Serial.println("ERROR: Receiver not found");
    refreshList();
    return;
  }

  selectReceiver(peer);
}

void ControllerMenu::selectReceiver(Peer* peer) {
  switch (espnowController.validateSelectedPeer(peer)) {
    case ALREADY_SELECTED:
      return;
    case PEER_ERROR:
      app.setStatusLabelText("PEER ERROR");
      refreshList();
      return;
    default:
      break;
  }
  updateStatus();
  app.setStatusLabelText("PAIRING...");
  switch (espnowController.sendPairRequest(peer)) {
    case PAIR_ERROR:
      app.setStatusLabelText("PAIR FAILED");
      refreshList();
      return;
    case PAIR_SUCCESS:
      app.setStatusLabelText("PAIR SUCCESS");
      toggle();
      break;
    default:
      break;
  }
}

void ControllerMenu::updateStatus() {
  if (!status_label) return;
  if (espnowController.hasActiveReceiver()) {
    app.setStatusLabelText("No Receiver");
  } else {
    Peer* peer = espnowController.getActiveReceiverPeer();
    if (peer) {
      char text[64];
      snprintf(text, sizeof(text), "CTRL: %s", peer->getName());
      app.setStatusLabelText(text);
    }
  }
}

void ControllerMenu::toggle() {
  if (lv_obj_is_hidden(menu_obj)) {
    lv_obj_move_foreground(menu_obj);

    lv_obj_set_hidden(menu_obj, false);
    espnowController.setReceiverListNeedsRefresh(true);
  } else {
    lv_obj_set_hidden(menu_obj, true);
  }
}