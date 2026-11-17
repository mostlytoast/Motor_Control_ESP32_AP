// ControllerMenu.h / .cpp
#include "ControllerMenu.h"

// struct ReceiverEventData {
//   Display* display;  // Keep reference to the main display if needed
//   uint8_t mac[6];
// };

ControllerMenu::ControllerMenu(lv_obj_t* screen) { createMenu(screen); }
void ControllerMenu::createMenu(lv_obj_t* screen) {
  root_ = lv_obj_create(screen);

  lv_obj_remove_style_all(root_);

  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_align(root_, LV_ALIGN_TOP_MID, 0, 0);

  // lv_obj_set_scrollbar_mode(list_obj, LV_STATE_DISABLED);

  // lv_obj_set_scroll_edge(root_, LV_SCROLL_EDGE_NONE);

  lv_obj_set_style_bg_color(root_, lv_color_hex(colorConfig.backgroundColor),
                            LV_PART_MAIN);

  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
  // lv_obj_set_style_border_color(root_, lv_color_hex(colorConfig.statusColor),
  //                               LV_PART_MAIN);
  // lv_obj_set_style_border_width(root_, 2, LV_PART_MAIN);

  lv_obj_set_scrollable(root_, false);
  optionsMenuDisplay_ = new OptionsMenu(root_, [this]() {});
  // Header
  lv_obj_t* header = lv_label_create(root_);
  sharedDisplay.createLabel(header, lv_color_hex(colorConfig.textColor), 0, 0,
                            LV_PART_MAIN, LV_ALIGN_TOP_MID, "SELECT RECEIVER");

  list_obj = lv_obj_create(root_);
  // Increase the height (250) to ensure there is room for multiple buttons
  lv_obj_set_size(list_obj, 290, 250);
  lv_obj_align(list_obj, LV_ALIGN_TOP_MID, 0, 10);
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

  hide_button = sharedDisplay.createActionButton(
      root_, "Hide", 215, 27, 100, 25, [this](lv_event_t* event) { toggle(); },
      lv_color_hex(colorConfig.menuHeaderColor));

  lv_obj_set_hidden(root_, true);
}

void ControllerMenu::refreshList() {
  if (lv_obj_is_hidden(root_)) {
    return;
  }
  lv_obj_clean(list_obj);

  // Scan button

  sharedDisplay.createActionButton(
      list_obj, "SCAN / DISCOVER", 220, 30, 250, 40,
      [this](lv_event_t* event) { this->scanEvent(nullptr); },
      lv_color_hex(colorConfig.colorScan));

  // Receiver buttons
  for (const Peer& peer : espnowController.getPeers()) {
    // Copy MAC address while the peer is valid.
    uint8_t mac[6];
    memcpy(mac, peer.getMac(), sizeof(mac));
    char text[64];

    bool selected = peer.getState() == PeerState::ACTIVE;
    snprintf(text, sizeof(text), "%s%s", selected ? "> " : "", peer.getName());

    lv_obj_t* button = sharedDisplay.createActionButton(
        list_obj, text, 270, 36, 210, 40,
        [this, mac](lv_event_t* event) { this->selectReceiverByMac(mac); },
        lv_color_hex(colorConfig.colorScan),
        [peer]() { return peer.getState() == PeerState::ACTIVE; });
    lv_obj_t* settings = sharedDisplay.createActionButton(
        list_obj, "settings", 220, 36, 40, 40,
        [this, mac](lv_event_t* event) { optionsMenuDisplay_->toggle(); },
        lv_color_hex(colorConfig.colorScan),
        [peer]() { return peer.getState() == PeerState::ACTIVE; });
  }

  if (espnowController.getNumberOfReceivers() == 0) {
    lv_obj_t* none = lv_label_create(list_obj);

    lv_label_set_text(none, "No receivers found");

    lv_obj_set_style_text_color(none, lv_color_hex(colorConfig.infoColor),
                                LV_PART_MAIN);
  }
}

// Logic methods
void ControllerMenu::scanEvent(lv_event_t* event) {
  espnowController.removePeers();
  espnowController.resetDiscover();

  app.setStatusLabelText("NO PEER");

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
      app.setStatusLabelText("ERROR");
      refreshList();
      return;
    default:
      break;
  }
  updateStatus();
  app.setStatusLabelText("PAIRING...");
  if (espnowController.sendPairRequest(peer)) {
    app.setStatusLabelText("PAIRED");
    toggle();
    return;
  } else {
    app.setStatusLabelText("FAILED");
    refreshList();
    return;
  }
}

void ControllerMenu::updateStatus() {
  if (!status_label) return;
  if (espnowController.hasActiveReceiver()) {
    app.setStatusLabelText("NO PEER");
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
  if (lv_obj_is_hidden(root_)) {
    lv_obj_move_foreground(root_);
    // lv_obj_move_to_index(root_, 0);

    lv_obj_set_hidden(root_, false);
    espnowController.setReceiverListNeedsRefresh(true);
  } else {
    lv_obj_set_hidden(root_, true);
    // lv_obj_move_to_index(root_, 2);
  }
}