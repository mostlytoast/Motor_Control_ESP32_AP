#include "EspnowController.h"

EspnowController espnowController;
EspnowController* EspnowController::callbackInstance = nullptr;
namespace {
JsonDocument previousState;
JsonDocument currentState;
bool hasReceivedInitialState = false;
bool hasSentInitialState = false;
}  // namespace

// SETUP

bool EspnowController::setupESPNow(
    GetJsonFromStateCallback getJsonFromStateCallback_,
    GetStateFromJsonCallback getStateFromJsonCallback_,
    ResetStateCallback resetStateCallback_,
    UpdateStateCallback updateStateCallback_, bool wifi) {
  getJsonFromStateCallback = getJsonFromStateCallback_;
  getStateFromJsonCallback = getStateFromJsonCallback_;
  resetStateCallback = resetStateCallback_;
  updateStateCallback = updateStateCallback_;

  callbackInstance = this;
  Serial.println("Initializing ESP-NOW");

  // WiFi

  if (wifi) {
    WiFi.mode(WIFI_STA);

    delay(100);

    WiFi.disconnect();

    delay(100);
  }

  // MAC

  esp_err_t result = esp_wifi_get_mac(WIFI_IF_STA, localMac);

  if (result != ESP_OK) {
    Serial.print("Failed to get WiFi MAC: ");

    Serial.println(esp_err_to_name(result));

    return false;
  }

  Serial.print("ESP-NOW MAC: ");

  Serial.println(macToString(localMac));

  // Channel

  result = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (result != ESP_OK) {
    Serial.print("Failed to set WiFi channel: ");

    Serial.println(esp_err_to_name(result));

    return false;
  }

  // ESP-NOW

  result = esp_now_init();

  if (result != ESP_OK) {
    Serial.print("ESP-NOW initialization FAILED: ");

    Serial.println(esp_err_to_name(result));

    return false;
  }

  Serial.println("ESP-NOW initialized");

  // Event queue
  auto cleanupESPNow = [this]() {
    if (espNowEventQueue != nullptr) {
      vQueueDelete(espNowEventQueue);
      espNowEventQueue = nullptr;
    }

    esp_now_deinit();
    callbackInstance = nullptr;
  };
  espNowEventQueue = xQueueCreate(ESPNOW_EVENT_QUEUE_SIZE, sizeof(EspNowEvent));

  if (espNowEventQueue == nullptr) {
    Serial.println("ERROR: Could not create ESP-NOW event queue");

    esp_now_deinit();
    cleanupESPNow();
    return false;
  }

  // Receive callback

  result = esp_now_register_recv_cb(onESPNowReceive);

  if (result != ESP_OK) {
    Serial.print("Failed to register receive callback: ");

    Serial.println(esp_err_to_name(result));

    esp_now_deinit();
    cleanupESPNow();

    return false;
  }

  // Send callback

  result = esp_now_register_send_cb(onESPNowSendActual);

  if (result != ESP_OK) {
    Serial.print("Failed to register send callback: ");

    Serial.println(esp_err_to_name(result));

    esp_now_deinit();
    cleanupESPNow();

    return false;
  }

  // Broadcast peer

  if (!esp_now_is_peer_exist(broadcastMac)) {
    esp_now_peer_info_t peerInfo = {};

    memcpy(peerInfo.peer_addr, broadcastMac, 6);

    peerInfo.channel = WIFI_CHANNEL;

    peerInfo.ifidx = WIFI_IF_STA;

    peerInfo.encrypt = false;

    result = esp_now_add_peer(&peerInfo);

    if (result != ESP_OK && result != ESP_ERR_ESPNOW_EXIST) {
      Serial.print("Failed to add broadcast peer: ");

      Serial.println(esp_err_to_name(result));

      esp_now_deinit();
      cleanupESPNow();

      return false;
    }
  }
  resetDiscover();

  Serial.println("ESP-NOW READY");

  return true;
}

// MAIN UPDATE

void EspnowController::espnowUpdate() {
  jsonAssembler.cleanupJsonReassemblies();

  processESPNowEvents();

  processDiscovery();

  ESPNowCheckFailsafe();

  processStateChanges();

  // TODO have an isController marker?
  sendHeartbeat();
}

void EspnowController::processESPNowEvents() {
  if (espNowEventQueue == nullptr) {
    return;
  }

  EspNowEvent event;

  int processed = 0;

  while (processed < 8 &&
         xQueueReceive(espNowEventQueue, &event, 0) == pdTRUE) {
    processed++;

    Peer temporaryPeer(event.mac, WIFI_CHANNEL, "");

    Peer* peer = peers.findPeer(event.mac);

    if (peer == nullptr) {
      peer = &temporaryPeer;
    }

    //
    // TODO MAJOR have to decide if we want to pass peer or mac through?
    // RECEIVE
    if (event.type == ESP_NOW_EVENT_RECEIVE) {
      if (event.len <= 0) {
        continue;
      }

      if (peer == nullptr) continue;
      // Binary packets

      if (processReceivedPeerBinaryPacket(peer, event.data, event.len)) {
        continue;
      }

      if (processReceivedControllerBinaryPacket(peer, event.data, event.len)) {
        continue;
      }

      // JSON

      if (processReceivedJsonFragment(peer, event.data, event.len)) {
        continue;
      }

      // 4. Unknown packet
      handleUnkownPacket(peer, event);
    }

    // =================================================
    // SEND CALLBACK EVENT
    // =================================================

    else if (event.type == ESP_NOW_EVENT_SEND) {
      // SEND FAILED

      if (event.sendStatus == ESP_NOW_SEND_FAIL && peer != nullptr) {
        Serial.println("esp now fail");

        // Controller-side receiver
        // Todo put into a controller function

        std::cout << *peer << "\n";

        peer->setLastSeen(millis());
        bool wasKnown = peer->getRole() != PeerRole::UNKNOWN;
        uint8_t mac[6];
        memcpy(mac, peer->getMac(), sizeof(mac));

        peers.removePeer(mac);

        if (esp_now_is_peer_exist(mac)) {
          esp_now_del_peer(mac);
        }

        resetDiscover();
      }

      // SEND SUCCESS

      else if (event.sendStatus == ESP_NOW_SEND_SUCCESS && peer != nullptr) {
        peer->setLastSeen(millis());
      }
    }
  }
}

void EspnowController::handleUnkownPacket(Peer* peer, EspNowEvent& event) {
  Serial.printf("Unknown ESP-NOW packet from %s ( %d bytes)\n",
                macToString(peer->getMac()), event.len);
  // TODO unpair
  ESPNowCommand command = {};
  command.type = CMD_UNPAIR_REQUEST;
  // command.value = 0;
  sendCommandToPeer(peer, command);
}

// DISCOVERY UPDATE

void EspnowController::processDiscovery() {
  if (!discoveryActive) return;

  const unsigned long now = millis();

  if (now - lastDiscoverySend < DISCOVERY_INTERVAL) {
    return;
  }

  lastDiscoverySend = now;

  ESPNowCommand command{};
  command.type = CMD_DISCOVER;

  esp_now_send(broadcastMac, reinterpret_cast<const uint8_t*>(&command),
               sizeof(command));
}
// FAILSAFE

void EspnowController::ESPNowCheckFailsafe() {
  std::vector<Peer> controllers = peers.getActiveControllerPeers();
  bool recentlyTimedOut = false;
  const unsigned long now = millis();

  for (const Peer& peer : controllers) {
    if (now - peer.getLastSeen() > ESPNOW_TIMEOUT) {
      Serial.println("================================");
      Serial.println("ESP-NOW CONTROLLER TIMEOUT");
      Serial.print("Controller: ");
      Serial.println(peer.getName());
      Serial.print("MAC: ");
      Serial.println(macToString(peer.getMac()));
      Serial.println("Removing controller...");
      Serial.println("================================");

      // Copy MAC before removing the peer.
      uint8_t mac[6];
      memcpy(mac, peer.getMac(), 6);

      // Remove from ESP-NOW.
      if (esp_now_is_peer_exist(mac)) {
        esp_now_del_peer(mac);
      }

      // Remove from your peer manager.
      peers.removePeer(mac);
      recentlyTimedOut = true;
      previousState = {};
      currentState = {};
      hasReceivedInitialState = false;
      hasSentInitialState = false;
    }
  }
  if (peers.getActiveControllerPeers().size() == 0 && recentlyTimedOut) {
    if (resetStateCallback != nullptr) resetStateCallback();
  }
  controllers.clear();

  // Check AFTER iterating.
}
void EspnowController::processStateChanges() {
  // Todo need to fix peers.getActivePeers() and use it here
  for (Peer& peer : peers.getActivePeers()) {  // Changed Peer* to Peer&

    if (sendServerState(&peer) && updateStateCallback) {
      updateStateCallback();
    }
  }
}

bool EspnowController::sendServerState(Peer* peer) {
  if (peer == nullptr) {
    return false;
  }

  JsonDocument fullState;
  getJsonFromStateCallback(fullState);

  // Make sure we actually have a JSON object.
  if (fullState.isNull()) {
    return false;
  }

  JsonDocument send;

  bool isFullUpdate = !hasSentInitialState;

  if (isFullUpdate) {
    // First transmission: send everything.
    send["data"] = fullState;

  } else {
    // Subsequent transmissions: only send changes.
    JsonDocument delta = JsonChangeFilter::createDelta(
        previousState.as<JsonVariantConst>(), fullState.as<JsonVariantConst>());

    if (delta.as<JsonObject>().size() == 0) {
      return false;
    }

    send["data"] = delta;
  }

  send["type"] = CMD_SERVER_STATE;
  send["full"] = isFullUpdate;

  bool success = sendJsonDocument(peer, send);

  // Only update our previous state if transmission succeeds.
  if (success) {
    previousState.set(fullState);
    hasSentInitialState = true;
  }

  return success;
}

// PEER MANAGEMENT

// TODO unify names of peer receiver and controller to be peer?? maybe not
bool EspnowController::addPeer(const uint8_t* mac, uint8_t channel,
                               const char* name) {
  // Keep the existing implementation.
  Peer peer = Peer(mac, channel, name);
  return addPeer(&peer);
}

bool EspnowController::addPeer(Peer* peer) {
  if (peer == nullptr || peer->getMac() == nullptr) {
    Serial.println("Cannot add null peer");
    return false;
  }

  // First, ensure the peer exists in our local list.
  Peer* storedPeer = peers.findPeer(peer->getMac());

  if (storedPeer == nullptr) {
    storedPeer = peers.addPeer(peer);

    if (storedPeer == nullptr) {
      Serial.println("Failed to add peer to local list");
      return false;
    }
  } else {
    storedPeer->setRole(peer->getRole());
  }

  // Now ensure the peer exists in ESP-NOW.
  if (esp_now_is_peer_exist(storedPeer->getMac())) {
    return true;
  }

  esp_now_peer_info_t peerInfo = {};

  memcpy(peerInfo.peer_addr, storedPeer->getMac(), 6);
  peerInfo.channel = storedPeer->getChannel();
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;
  esp_err_t result = esp_now_add_peer(&peerInfo);

  if (result != ESP_OK && result != ESP_ERR_ESPNOW_EXIST) {
    Serial.printf("ESP-NOW add peer failed -> %s, free mem %s",
                  macToString(storedPeer->getMac()), ESP.getFreeHeap());
    return false;
  }

  return true;
}

bool EspnowController::addOrUpdateDiscoveredPeer(Peer* peer,
                                                 const ESPNowCommand* command) {
  if (command == nullptr || peer == nullptr) {
    return false;
  }

  // Existing receiver
  peer->setMac(command->mac);
  peer->setName((const char*)command->name);
  peer->setChannel(command->channel);
  peer->setLastSeen(millis());
  if (peers.findPeer(peer->getMac())) {
    return true;
  }

  // Receiver list full

  if (peers.getNumberOfPeers() >= MAX_PEERS) {
    Serial.println("Receiver list full");

    return false;
  }

  // New receiver

  peer->setLastSeen(millis());
  peer->setRole(PeerRole::UNKNOWN);
  peer->setState(PeerState::DISCOVERED);

  // TODO need way to clear out old receivers
  Serial.print("DISCOVERED PEER: ");
  Serial.print(peer->getName());
  Serial.print(" / ");
  Serial.println(macToString(peer->getMac()));

  if (!addPeer(peer)) {
    Serial.print("failed to add peer");
    peer->setState(PeerState::TIMED_OUT);
    return false;
  }

  return true;
}

// BINARY DISCOVERY RESPONSE

void EspnowController::sendDiscoveryResponse(Peer* peer) {
  ESPNowCommand response{};

  response.type = CMD_DISCOVERY_RESPONSE;

  uint8_t currentMac[6];

  WiFi.macAddress(currentMac);

  memcpy(response.mac, currentMac, 6);

  strncpy(response.name, NAME, sizeof(response.name) - 1);

  response.channel = WIFI_CHANNEL;

  if (!isBroadcastMac(peer->getMac())) {
    addPeer(peer);
  }

  sendCommandToPeer(peer, response);
}

// BINARY PAIR RESPONSE

void EspnowController::sendPairResponse(Peer* peer) {
  Serial.println("sending pair response");
  ESPNowCommand response = {};

  response.type = CMD_PAIR_RESPONSE;

  // response.value = 1;

  uint8_t currentMac[6];

  WiFi.macAddress(currentMac);

  memcpy(response.data + 0, currentMac, 6);

  response.data[6] = 1;

  strncpy((char*)response.data + 7, NAME, sizeof(NAME) - 1);

  sendCommandToPeer(peer, response);

  Serial.println("Pair response sent");
}

// GENERIC JSON SEND

bool EspnowController::sendJsonDocument(Peer* peer, JsonDocument& doc) {
  // Calculate the serialized JSON size
  const size_t jsonLength = measureJson(doc);

  const size_t requiredChunks =
      (jsonLength + JSON_FRAGMENT_DATA_SIZE - 1) / JSON_FRAGMENT_DATA_SIZE;

  const size_t packetBufferSize = requiredChunks * JSON_FRAGMENT_PACKET_SIZE;

  uint8_t* packets = static_cast<uint8_t*>(malloc(packetBufferSize));

  if (packets == nullptr) {
    Serial.println("Failed to allocate JSON packet buffer");

    return false;
  }

  size_t totalChunks = 0;
  // Generate packets
  if (jsonAssembler.disassembleJson(doc, packets, totalChunks)) {
    // TODO use object thats an array of packets
    // Read each packet
    for (size_t i = 0; i < totalChunks; i++) {
      ESPNowCommand command{};

      command.type = CMD_SERVER_STATE;

      uint8_t* packet = packets + (i * JSON_FRAGMENT_PACKET_SIZE);

      memcpy(command.data, packet, JSON_FRAGMENT_PACKET_SIZE);
      Serial.println("sending CMD_SERVER_STATE");
      if (!sendCommandToPeer(peer, command)) {
        Serial.println("failed sending CMD_SERVER_STATE");

        free(packets);
        return false;
      }
      // Give the ESP-NOW driver time to process the transmission.
      delay(5);
    }
  }
  free(packets);

  return true;
}

// PROCESS BINARY RECEIVER-SIDE PACKETS

bool EspnowController::processReceivedPeerBinaryPacket(Peer* peer,
                                                       const uint8_t* data,
                                                       int len) {
  if (peer == nullptr || data == nullptr || len != sizeof(ESPNowCommand)) {
    return false;
  }
  ESPNowCommand command = {};
  memcpy(&command, data, sizeof(command));

  // DISCOVERY
  if (command.type == CMD_DISCOVER) {
    sendDiscoveryResponse(peer);
    return true;
  }

  // PAIR REQUEST - UPDATED TO UPDATE LAST SEEN
  if (command.type == CMD_PAIR_REQUEST) {
    Serial.println("received CMD_PAIR_REQUEST");
    peer->setLastSeen(millis());
    peer->setState(PeerState::PAIRED);
    Serial.println("1213");

    addPeer(peer);
    Serial.println("124wesdf");

    sendPairResponse(peer);

    return true;
  }
  // HEART BEAT
  if (command.type == CMD_HEARTBEAT) {
    // Serial.println("received CMD_HEARTBEAT");

    if (peer != nullptr) {
      peer->setLastSeen(millis());
    }

    return true;
  }

  // UNPAIR - UPDATED TO UPDATE LAST SEEN

  if (command.type == CMD_UNPAIR_REQUEST) {
    Serial.println("received CMD_UNPAIR_REQUEST");

    if (esp_now_is_peer_exist(peer->getMac())) {
      esp_now_del_peer(peer->getMac());
    }
    peers.removePeer(peer->getMac());

    Serial.println("Controller unpaired");

    return true;
  }

  // CLAIM CONTROLLER - UPDATED TO UPDATE LAST SEEN
  if (command.type == CMD_CONTROLLER_CLAIM) {
    Serial.println("received CMD_CONTROLLER_CLAIM");

    peer->setLastSeen(millis());
    if (peer->getState() == PeerState::PAIRED) {
      discoveryActive = false;

      peer->setState(PeerState::ACTIVE);
      peer->setRole(PeerRole::CONTROLLER);
      peer->setName(command.name);
      peer->setLastSeen(millis());

      Serial.print("ACTIVE PEER -> ");

      Serial.println(peer->getName());
      addPeer(peer);
      sendServerState(peer);

    } else {
      Serial.println("Claim rejected: controller not paired");
    }
    return true;
  }
  return false;
}

bool EspnowController::sendControllerClaim(Peer* peer) {
  if (peer == nullptr || peer->getMac() == nullptr) {
    Serial.println("CLAIM: Invalid peer pointer or MAC");
    return false;
  }

  Serial.printf("Sending CMD_CONTROLLER_CLAIM to %s, MAC: %s\n",
                peer->getName(), macToString(peer->getMac()).c_str());

  ESPNowCommand command = {};
  command.type = CMD_CONTROLLER_CLAIM;
  snprintf(command.name, sizeof(command.name), "%s", peer->getName());
  // command.value = 0;
  return sendCommandToPeer(peer, command);
}

bool EspnowController::sendCommandToPeer(Peer* peer, ESPNowCommand command) {
  if (peer == nullptr || peer->getMac() == nullptr) {
    Serial.println("Invalid peer pointer or MAC");
    return false;
  }

  if (!esp_now_is_peer_exist(peer->getMac())) {
    if (!addPeer(peer)) {
      Serial.println("Failed to add receiver peer");
      return false;
    }
  }

  esp_err_t result =
      esp_now_send(peer->getMac(), reinterpret_cast<const uint8_t*>(&command),
                   sizeof(command));

  if (result != ESP_OK) {
    Serial.printf("send failed -> %s, free mem %s", macToString(peer->getMac()),
                  ESP.getFreeHeap());

    return false;
  }
  return true;
}

// PROCESS CONTROLLER-SIDE BINARY PACKETS

bool EspnowController::processReceivedControllerBinaryPacket(
    Peer* peer, const uint8_t* data, int len) {
  if (peer == nullptr || data == nullptr || len != sizeof(ESPNowCommand)) {
    Serial.println("response from unknown peer");
    return false;
  }

  // DISCOVERY RESPONSE - UPDATED TO UPDATE LAST SEEN

  ESPNowCommand command = {};
  memcpy(&command, data, sizeof(command));

  if (command.type == CMD_DISCOVERY_RESPONSE) {
    // Serial.println("sending CMD_DISCOVERY_RESPONSE");

    if (addOrUpdateDiscoveredPeer(peer, &command)) {
      receiverListNeedsRefresh = true;
    }

    // Update last seen for the peer that sent this
    peer->setLastSeen(millis());

    return true;
  }

  // Pair response - UPDATED TO UPDATE LAST SEEN

  if (command.type == CMD_PAIR_RESPONSE) {
    Serial.println("sending CMD_PAIR_RESPONSE");
    peer->setLastSeen(millis());

    if (command.data[0] == 0) {
      peer->setState(PeerState::TIMED_OUT);
      receiverListNeedsRefresh = true;
      Serial.println("Pairing rejected by receiver");
      return true;
    }

    peer->setState(PeerState::ACTIVE);
    receiverListNeedsRefresh = true;

    Serial.println("Pairing accepted; sending claim");

    if (!sendControllerClaim(peer)) {
      Serial.println("Failed to send controller claim");
      resetDiscover();
      // if (resetStateCallback) resetStateCallback();
    }

    Serial.println("Successfully sent controller claim");
    return true;
  }

  return false;
}

bool EspnowController::processReceivedJsonFragment(Peer* peer,
                                                   const uint8_t* data,
                                                   size_t len) {
  if (peer == nullptr || data == nullptr || len != sizeof(ESPNowCommand)) {
    Serial.println("response from unknown peer");
    return false;
  }

  ESPNowCommand command = {};
  memcpy(&command, data, sizeof(command));

  if (command.type != CMD_SERVER_STATE) {
    return false;
  }

  JsonDocument doc;

  if (!jsonAssembler.jsonFragmentToJson(peer->getMac(), command.data,
                                        sizeof(command.data), doc)) {
    Serial.println("failed to fragment");
    return false;
  }
  if (doc.isNull()) {
    Serial.println("[STATE] Waiting for remaining fragments");
    return true;  // Packet handled, not an unknown packet!
  }

  if (doc["data"].isNull()) {
    Serial.println("[STATE] Complete message has no data");
    return true;
  }
  if (peer->getState() != PeerState::PAIRED &&
      peer->getState() != PeerState::ACTIVE) {
    Serial.println("peer invalid ");

    return false;
  }

  peer->setLastSeen(millis());

  JsonVariantConst incoming = doc["data"];

  if (incoming.isNull()) {
    Serial.println("data invalid ");

    return false;
  }

  bool isFullUpdate = doc["full"] | false;

  if (isFullUpdate) {
    // Replace the entire existing state.
    currentState.set(incoming);

    hasReceivedInitialState = true;

  } else {
    if (!hasReceivedInitialState) {
      Serial.println(
          "[STATE] Delta received before initial full state. Ignoring.");
      return true;
    }

    // Merge changes into the existing state.
    JsonChangeFilter::applyDelta(currentState.as<JsonVariant>(), incoming);
  }

  getStateFromJsonCallback(currentState);

  return true;
}

void EspnowController::sendHeartbeat() {
  static unsigned long lastHeartbeat = 0;

  const unsigned long now = millis();

  if (now - lastHeartbeat < HEARTBEAT_INTERVAL) {
    return;
  }

  lastHeartbeat = now;

  ESPNowCommand command;
  command.type = CMD_HEARTBEAT;
  for (Peer& peer : peers.getActiveReceiverPeers()) {
    sendCommandToPeer(&peer, command);
  }
}
validateSelectedPeerState EspnowController::validateSelectedPeer(Peer* peer) {
  if (peer == nullptr) {
    return PEER_ERROR;
  }

  if (peer->getIsActivePeer()) {
    return ALREADY_SELECTED;
  }
  if (!addPeer(peer)) {
    return PEER_ERROR;
  }
  if (esp_now_is_peer_exist(peer->getMac())) {
    return PASS_CHECK;
  }
  return PASS_CHECK;
}

bool EspnowController::sendPairRequest(Peer* peer) {
  Serial.printf("Sending CMD_PAIR_REQUEST to %s, MAC: %s \n", peer->getName(),
                macToString(peer->getMac()).c_str());

  ESPNowCommand pairPacket = {};

  pairPacket.type = CMD_PAIR_REQUEST;

  memcpy(pairPacket.data, localMac, 6);
  bool result = sendCommandToPeer(peer, pairPacket);
  peer->setRole(PeerRole::RECEIVER);

  Serial.printf("PAIR request successfully sent\n");

  return result;
}

void EspnowController::resetDiscover() {
  // Reset receiver list
  Serial.println("resetDiscover");

  resetStateCallback();

  discoveryActive = true;

  discoveryStarted = millis();

  lastDiscoverySend = 0;

  receiverListNeedsRefresh = true;

  lastHeartbeatSend = millis();
}

void EspnowController::removePeers() { peers.removePeers(); }

int EspnowController::getNumberOfReceivers() {
  return peers.getNumberOfPeers();
}
void EspnowController::setReceiverListNeedsRefresh(bool enable) {
  receiverListNeedsRefresh = enable;
}
bool EspnowController::getReceiverListNeedsRefresh() {
  return receiverListNeedsRefresh;
}

bool EspnowController::hasActiveReceiver() {
  return peers.getActivePeers().size() > 0;
}

std::vector<Peer> EspnowController::getActiveControllerPeers() {
  return peers.getActiveControllerPeers();
}

std::vector<Peer> EspnowController::getActiveReceiverPeers() {
  return peers.getActiveReceiverPeers();
}

std::vector<Peer> EspnowController::getPeers() { return peers.getPeers(); }

Peer* EspnowController::getActiveReceiverPeer() {
  return peers.getActiveReceiverPeer();
}

// EVENT QUEUE

void EspnowController::queueEspNowEvent(EspNowEventType type,
                                        const uint8_t* mac, const uint8_t* data,
                                        int len) {
  if (espNowEventQueue == nullptr) {
    return;
  }

  EspNowEvent event{};

  event.type = type;

  if (mac != nullptr) {
    memcpy(event.mac, mac, 6);
  }

  if (data != nullptr && len > 0) {
    len = std::min(len, ESPNOW_EVENT_DATA_MAX);

    memcpy(event.data, data, len);
    event.len = len;
  }

  if (xQueueSend(espNowEventQueue, &event, 0) != pdPASS) {
    static uint32_t droppedEvents = 0;
    droppedEvents++;

    if (droppedEvents % 100 == 0) {
      Serial.printf("[ESP-NOW] Dropped events: %lu\n",
                    static_cast<unsigned long>(droppedEvents));
    }
  }
}
// RECEIVE CALLBACK

void EspnowController::onESPNowReceive(const esp_now_recv_info_t* info,
                                       const uint8_t* data, int len) {
  if (info == nullptr || data == nullptr || len <= 0 ||
      callbackInstance == nullptr) {
    return;
  }

  callbackInstance->queueEspNowEvent(ESP_NOW_EVENT_RECEIVE, info->src_addr,
                                     data, len);
}

// SEND CALLBACK
void EspnowController::onESPNowSendActual(const wifi_tx_info_t* tx_info,
                                          esp_now_send_status_t status) {
  if (tx_info == nullptr || callbackInstance == nullptr ||
      callbackInstance->espNowEventQueue == nullptr) {
    return;
  }

  EspNowEvent event{};

  memcpy(event.mac, tx_info->des_addr, 6);
  event.type = ESP_NOW_EVENT_SEND;
  event.sendStatus = status;

  if (xQueueSend(callbackInstance->espNowEventQueue, &event, 0) != pdPASS) {
    return;
  }
}