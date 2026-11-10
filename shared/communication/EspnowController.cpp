#include "EspnowController.h"

EspnowController espnowController;
EspnowController* EspnowController::callbackInstance = nullptr;
namespace {
JsonDocument previousState;
bool previousStateUpdated = false;
}  // namespace

// SETUP

bool EspnowController::setupESPNow(
    GetJsonFromStateCallback getJsonFromStateCallback_,
    GetStateFromJsonCallback getStateFromJsonCallback_,
    ResetStateCallback resetStateCallback_, bool wifi) {
  getJsonFromStateCallback = getJsonFromStateCallback_;
  getStateFromJsonCallback = getStateFromJsonCallback_;
  resetStateCallback = resetStateCallback_;

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

  espNowEventQueue = xQueueCreate(ESPNOW_EVENT_QUEUE_SIZE, sizeof(EspNowEvent));

  if (espNowEventQueue == nullptr) {
    Serial.println("ERROR: Could not create ESP-NOW event queue");

    esp_now_deinit();

    return false;
  }

  // Receive callback

  result = esp_now_register_recv_cb(onESPNowReceive);

  if (result != ESP_OK) {
    Serial.print("Failed to register receive callback: ");

    Serial.println(esp_err_to_name(result));

    esp_now_deinit();

    return false;
  }

  // Send callback

  result = esp_now_register_send_cb(onESPNowSendActual);

  if (result != ESP_OK) {
    Serial.print("Failed to register send callback: ");

    Serial.println(esp_err_to_name(result));

    esp_now_deinit();

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

      return false;
    }
  }

  discoveryActive = true;

  discoveryStarted = millis();

  lastDiscoverySend = 0;

  lastHeartbeatSend = millis();

  pairingModeStartedTime = millis();

  Serial.println();
  Serial.println("ESP-NOW READY");

  Serial.println("Controller + receiver communication enabled");

  Serial.println();

  return true;
}

// MAIN UPDATE

void EspnowController::espnowUpdate() {
  jsonAssembler.cleanupJsonReassemblies();

  processESPNowEvents();

  processDiscovery();

  ESPNowUpdatePairingMode();

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
    // Peer peer = Peer(event.mac, WIFI_CHANNEL, "");
    // TODO MAJOR have to decide if we want to pass peer or mac through?
    // RECEIVE
    if (event.type == ESP_NOW_EVENT_RECEIVE) {
      if (event.len <= 0) {
        continue;
      }

      // Binary packets

      if (processReceivedPeerBinaryPacket(event.mac, event.data, event.len)) {
        continue;
      }

      if (processReceivedControllerBinaryPacket(event.mac, event.data,
                                                event.len)) {
        continue;
      }

      // JSON

      if (processReceivedJsonFragment(event.mac, event.data, event.len)) {
        continue;
      }

      // 4. Unknown packet
      Serial.printf("Unknown ESP-NOW packet from %s ( %d bytes)",
                    macToString(event.mac), event.len);
      // TODO unpair
      ESPNowCommand command = {};
      command.type = CMD_UNPAIR_REQUEST;
      command.value = 0;

      esp_err_t result =
          esp_now_send(event.mac, reinterpret_cast<const uint8_t*>(&command),
                       sizeof(command));
    }

    // =================================================
    // SEND CALLBACK EVENT
    // =================================================

    else if (event.type == ESP_NOW_EVENT_SEND) {
      // SEND FAILED
      Peer* peer = peers.findPeer(event.mac);

      if (event.sendStatus == ESP_NOW_SEND_FAIL && peer != nullptr) {
        Serial.printf("ESP-NOW send failed -> %s", macToString(event.mac));
        Serial.print("free memory");
        Serial.println(ESP.getFreeHeap());
        // Controller-side receiver
        // Todo put into a controller function
        app.setStatusLabelText("Fail");
        Serial.println("peer timed out");
        std::cout << *peer << "\n";
        peer->setLastSeen(millis());
        discoveryActive = true;
        if (peer->getIsActivePeerReceiver()) {
          ResetStateCallback();
          peer->setState(PeerState::TIMED_OUT);
          // TODO remove peer from list
        }
        // todo have to set app states to defaults
        // TODO probs dont wanna remove here only let failsafe remove stuff 
        peers.removePeer(peer->getMac());
        esp_now_del_peer(peer->getMac());
      }

      // SEND SUCCESS

      else if (event.sendStatus == ESP_NOW_SEND_SUCCESS && peer != nullptr) {
        peer->setLastSeen(millis());
      }
    }
  }
}

// DISCOVERY UPDATE

void EspnowController::processDiscovery() {
  if (!discoveryActive) return;
  unsigned long now = millis();

  if (now - lastDiscoverySend >= DISCOVERY_INTERVAL) {
    lastDiscoverySend = now;

    ESPNowCommand command = {};

    command.type = CMD_DISCOVER;

    command.value = 0;

    esp_err_t result = esp_now_send(
        broadcastMac, reinterpret_cast<uint8_t*>(&command), sizeof(command));

    if (result != ESP_OK) {
      Serial.printf("Discovery send failed: %d / %s\n", result,
                    esp_err_to_name(result));
      Serial.print("free memory");
      Serial.println(ESP.getFreeHeap());
    }
  }
}

// PAIRING MODE TIMEOUT

void EspnowController::ESPNowUpdatePairingMode() {
  // TODO do i even need this maybe remove
  if (pairingMode && pairingModeStartedTime != 0 &&
      millis() - pairingModeStartedTime > DEFAULT_PAIRING_WINDOW_MS) {
    pairingMode = false;

    Serial.println("Pairing window expired");
  }
}

// FAILSAFE

void EspnowController::ESPNowCheckFailsafe() {
  for (Peer& peer : peers.getActiveControllerPeers()) {
    if (millis() - peer.getLastSeen() > ESPNOW_TIMEOUT) {
      Serial.println("ESP-NOW TIMEOUT - MOTOR STOP");
      peers.removePeer(&peer);
      esp_now_del_peer(peer.getMac());

      // TODO remove from espnow list

      // resetDiscover();

      //   // TODO remove this app. refs?
      //   // app.setTargetSpeed(0);

      //   // app.setAutoState(false);

      //   // app.setAutoMode(AUTO_IDLE);

      // espNowFailsafeActive = true;
    } else if (peers.getActiveControllerPeers().size() == 0) {
      ResetStateCallback();
      resetDiscover();
      espNowFailsafeActive = true;
    }
  }
}
void EspnowController::processStateChanges() {
  // Todo need to fix peers.getActivePeers() and use it here
  for (const Peer& peer : peers.getActivePeers()) {
    sendServerState(peer);
  }
}
bool EspnowController::sendServerState(const Peer& peer) {
  JsonDocument fullState;

  getJsonFromStateCallback(fullState);
  if (fullState.isNull()) {
    return false;
  }

  if (!previousStateUpdated) {
    previousState = fullState;  // Ensure previousState is a JsonDocument type
    previousStateUpdated = true;
    return false;  // Return false because nothing was sent yet
  }

  if (previousState == fullState) {
    return false;
  }

  previousState = fullState;
  // Serial.println("actually sent state");
  // fullState["type"] = CMD_SERVER_STATE;

  // return sendJsonDocument(&peer, fullState);

  JsonDocument send;
  send["data"] = fullState;
  send["type"] = CMD_SERVER_STATE;
  return sendJsonDocument(&peer, send);
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
    Serial.print("ESP-NOW add peer failed: ");
    Serial.println(esp_err_to_name(result));
    return false;
  }

  return true;
}

bool EspnowController::addOrUpdateReceiver(const DiscoveryPacket* packet) {
  if (packet == nullptr) {
    return false;
  }

  Peer* peer = peers.findPeer(packet->receiverMac);

  // Existing receiver

  if (peer != nullptr) {
    peer->setMac(packet->receiverMac);
    peer->setName((const char*)packet->receiverName);
    peer->setChannel(packet->channel);
    peer->setLastSeen(millis());

    return true;
  }

  // Receiver list full

  if (peers.getNumberOfPeers() >= MAX_RECEIVERS) {
    Serial.println("Receiver list full");

    return false;
  }

  // New receiver

  Peer receiverPeer = Peer(packet->receiverMac, packet->channel,
                           reinterpret_cast<const char*>(packet->receiverName));
  receiverPeer.setLastSeen(millis());

  receiverPeer.setRole(PeerRole::RECEIVER);
  receiverPeer.setState(PeerState::DISCOVERED);

  // TODO need way to clear out old receivers
  Serial.print("DISCOVERED RECEIVER: ");
  Serial.print(receiverPeer.getName());
  Serial.print(" / ");
  Serial.println(macToString(receiverPeer.getMac()));

  if (!addPeer(&receiverPeer)) {
    Serial.print("failed to add receiver");
    receiverPeer.setState(PeerState::TIMED_OUT);
    return false;
  }

  return true;
}

// PAIR CONTROLLER

bool EspnowController::claimPeer(Peer* peer) {
  peer->setState(PeerState::ACTIVE);
  peer->setRole(PeerRole::CONTROLLER);

  peer->setLastSeen(millis());

  Serial.print("ACTIVE PEER -> ");

  Serial.println(peer->getName());

  return true;
}

// BINARY DISCOVERY RESPONSE

void EspnowController::sendDiscoveryPacket(const uint8_t* mac) {
  DiscoveryPacket response = {};

  response.type = CMD_DISCOVERY_RESPONSE;

  response.value = MAX_CONTROLLERS;

  uint8_t receiverMac[6];

  WiFi.macAddress(receiverMac);

  memcpy(response.receiverMac, receiverMac, 6);

  uint8_t count = 0;
  for (const Peer& peer : peers.getActiveControllerPeers()) {
    if (peer.getState() != PeerState::TIMED_OUT) {
      count++;
    }
  }

  response.pairedCount = count;

  response.pairingMode = pairingMode ? 1 : 0;

  strncpy(response.receiverName, RECEIVER_NAME,
          sizeof(response.receiverName) - 1);

  response.channel = WIFI_CHANNEL;

  if (!isBroadcastMac(mac)) {
    addPeer(mac, WIFI_CHANNEL, response.receiverName);
  }

  esp_err_t result = esp_now_send(mac, reinterpret_cast<uint8_t*>(&response),
                                  sizeof(response));
}

// BINARY PAIR RESPONSE

void EspnowController::sendPairResponse(const uint8_t* mac, bool paired) {
  ESPNowCommand response = {};

  response.type = CMD_PAIR_RESPONSE;

  response.value = paired ? 1 : 0;

  uint8_t receiverMac[6];

  WiFi.macAddress(receiverMac);

  memcpy(response.data + 0, receiverMac, 6);

  response.data[6] = paired ? 1 : 0;

  strncpy((char*)response.data + 7, RECEIVER_NAME, sizeof(RECEIVER_NAME) - 1);

  addPeer(mac, WIFI_CHANNEL, RECEIVER_NAME);

  esp_err_t result = esp_now_send(mac, reinterpret_cast<uint8_t*>(&response),
                                  sizeof(response));

  if (result != ESP_OK) {
    Serial.print("Pair response send failed: ");

    Serial.print(result);

    Serial.print(" / ");

    Serial.println(esp_err_to_name(result));
    Serial.print("free memory");
    Serial.println(ESP.getFreeHeap());
    return;
  }
  Serial.println("Pair response sent");
}

// BINARY CONTROLLER STATUS

void EspnowController::sendControllerStatus(Peer* peer) {
  if (peer == nullptr) {
    Serial.println("peer is null");

    return;
  }

  ESPNowCommand response = {};

  response.type = CMD_CONTROLLER_STATUS;

  response.value = -1;

  if (peer->getState() == PeerState::PAIRED &&
      peer->getIsActivePeerController()) {
    memcpy(response.data, peer->getMac(), 6);
  }

  addPeer(peer);

  esp_now_send(peer->getMac(), reinterpret_cast<uint8_t*>(&response),
               sizeof(response));
}

// GENERIC JSON SEND
//
// Automatically fragments any JSON document into
// packets <= 250 bytes.
//
// The receiver reconstructs the original JSON before
// deserializing it.

bool EspnowController::sendJsonDocument(const Peer* peer, JsonDocument& doc) {
  if (peer == nullptr) {
    return false;
  }
  const uint8_t* destination = peer->getMac();
  if (destination == nullptr) {
    return false;
  }

  // Make sure peer exists

  if (!esp_now_is_peer_exist(destination)) {
    return false;
  }

  // Calculate the serialized JSON size
  const size_t jsonLength = measureJson(doc);

  const size_t requiredChunks =
      (jsonLength + JSON_FRAGMENT_DATA_SIZE - 1) / JSON_FRAGMENT_DATA_SIZE;

  const size_t packetBufferSize = requiredChunks * JSON_FRAGMENT_PACKET_SIZE;

  uint8_t* packets = static_cast<uint8_t*>(malloc(packetBufferSize));

  if (packets == nullptr) {
    Serial.println("Failed to allocate JSON packet buffer");
    free(packets);

    return false;
  }

  size_t totalChunks = 0;
  // Generate packets
  if (jsonAssembler.disassembleJson(doc, packets, totalChunks)) {
    // TODO use object thats an array of packets
    // Read each packet
    for (size_t i = 0; i < totalChunks; i++) {
      uint8_t* packet = packets + (i * JSON_FRAGMENT_PACKET_SIZE);

      esp_err_t result =
          esp_now_send(destination, packet, JSON_FRAGMENT_PACKET_SIZE);

      if (result != ESP_OK) {
        Serial.print("JSON fragment send failed: ");
        Serial.print(esp_err_to_name(result));
        Serial.print("free memory");
        Serial.println(ESP.getFreeHeap());

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

bool EspnowController::processReceivedPeerBinaryPacket(const uint8_t* mac,
                                                       const uint8_t* data,
                                                       int len) {
  if (mac == nullptr || data == nullptr || len <= 0) {
    return false;
  }

  // DISCOVERY
  if (len >= 1 && data[0] == CMD_DISCOVER) {
    sendDiscoveryPacket(mac);
    return true;
  }

  // PAIRING PACKETS REQUIRE ENOUGH ROOM
  if (len != sizeof(ESPNowCommand)) {
    return false;
  }

  ESPNowCommand command = {};
  memcpy(&command, data, sizeof(command));

  // PAIR REQUEST - UPDATED TO UPDATE LAST SEEN
  if (command.type == CMD_PAIR_REQUEST) {
    Peer* peer = peers.findPeer(mac);
    if (peer != nullptr) {
      peer->setLastSeen(millis());
      peer->setState(PeerState::PAIRED);
    }
    sendPairResponse(mac, true);
    return true;
  }

  // UNPAIR - UPDATED TO UPDATE LAST SEEN
  Peer* peer = peers.findPeer(mac);
  if (command.type == CMD_UNPAIR_REQUEST) {
    if (peer != nullptr) {
      peer->setLastSeen(millis());
    }
    peers.removePeer(peer->getMac());
    esp_now_del_peer(peer->getMac());
    return true;
  }

  // CLAIM CONTROLLER - UPDATED TO UPDATE LAST SEEN
  if (command.type == CMD_CONTROLLER_CLAIM) {
    if (peer != nullptr) {
      peer->setLastSeen(millis());
    }
    if (peer->getState() == PeerState::PAIRED) {
      discoveryActive = false;

      if (peer != nullptr) {
        claimPeer(peer);
        sendControllerStatus(peer);
        sendServerState(*peer);
      }
    } else {
      Serial.println("Claim rejected: controller not paired");
    }
    return true;
  }

  // STATUS - UPDATED TO UPDATE LAST SEEN
  if (command.type == CMD_CONTROLLER_STATUS) {
    if (peer != nullptr && peer->getState() == PeerState::PAIRED) {
      peer->setLastSeen(millis());
    }
    sendControllerStatus(peer);
    return true;
  }

  return false;
}

bool EspnowController::sendControllerClaim(Peer* peer) {
  if (peer == nullptr || peer->getMac() == nullptr) {
    Serial.println("CLAIM: Invalid peer pointer or MAC");
    return false;
  }

  Serial.print("CLAIM destination MAC: ");
  Serial.println(macToString(peer->getMac()));
  if (peer->getState() != PeerState::ACTIVE) {
    Serial.println("CLAIM: Receiver is not paired");
    return false;
  }

  if (!esp_now_is_peer_exist(peer->getMac())) {
    if (!addPeer(peer)) {
      Serial.println("CLAIM: Failed to add receiver peer");
      return false;
    }
  }

  ESPNowCommand command = {};
  command.type = CMD_CONTROLLER_CLAIM;
  command.value = 0;

  esp_err_t result =
      esp_now_send(peer->getMac(), reinterpret_cast<const uint8_t*>(&command),
                   sizeof(command));

  Serial.print("Sending CMD_CONTROLLER_CLAIM to ");
  Serial.print(peer->getName());
  Serial.print(": ");
  Serial.println(esp_err_to_name(result));

  return result == ESP_OK;
}

// PROCESS CONTROLLER-SIDE BINARY PACKETS

bool EspnowController::processReceivedControllerBinaryPacket(
    const uint8_t* mac, const uint8_t* data, int len) {
  if (mac == nullptr || data == nullptr || len <= 0) {
    return false;
  }

  // DISCOVERY RESPONSE - UPDATED TO UPDATE LAST SEEN
  if (len == sizeof(DiscoveryPacket)) {
    DiscoveryPacket packet = {};
    memcpy(&packet, data, sizeof(packet));

    if (packet.type == CMD_DISCOVERY_RESPONSE) {
      if (addOrUpdateReceiver(&packet)) {
        receiverListNeedsRefresh = true;
      }

      // Update last seen for the peer that sent this
      Peer* sendingPeer = peers.findPeer(mac);
      if (sendingPeer != nullptr) {
        sendingPeer->setLastSeen(millis());
      }
      return true;
    }
  }

  // Pair response - UPDATED TO UPDATE LAST SEEN
  if (len == sizeof(ESPNowCommand)) {
    ESPNowCommand response = {};
    memcpy(&response, data, sizeof(response));

    if (response.type == CMD_PAIR_RESPONSE) {
      Peer* peer = peers.findPeer(mac);

      if (peer == nullptr) {
        Serial.println("Pair response from unknown peer");
        return true;
      }

      peer->setLastSeen(millis());

      if (response.data[0] == 0) {
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
      }

      Serial.println("Successfully sent controller claim");
      return true;
    }
  }

  // Controller status - UPDATED TO UPDATE LAST SEEN
  if (len == sizeof(ESPNowCommand)) {
    ESPNowCommand command = {};
    memcpy(&command, data, sizeof(command));

    if (command.type == CMD_CONTROLLER_STATUS) {
      Peer* sendingPeer = peers.findPeer(mac);
      if (sendingPeer != nullptr) {
        sendingPeer->setLastSeen(millis());
      }

      Serial.println("CONTROLLER STATUS RECEIVED");
      return true;
    }
  }

  return false;
}

bool EspnowController::processReceivedJsonFragment(const uint8_t* mac,
                                                   const uint8_t* data,
                                                   size_t len) {
  JsonDocument doc;
  JsonReassembly assembly;
  if (!jsonAssembler.jsonFragmentToJson(mac, data, len, doc)) {
    return false;
  }

  uint8_t type = doc["type"].as<uint8_t>();
  if (type == CMD_SERVER_STATE) {
    Peer* peer = peers.findPeer(mac);
    if (peer == nullptr || !(peer->getState() == PeerState::PAIRED ||
                             peer->getState() == PeerState::ACTIVE)) {
      return false;
    }
    peer->setLastSeen(millis());
    JsonDocument data = doc["data"];
    if (!data.isNull()) {
      getStateFromJsonCallback(data);
    }

  } else {
    Peer* peer = peers.findPeer(mac);
    if (peer != nullptr && peer->getIsActivePeer()) {
      // Record communication - UPDATED TO UPDATE LAST SEEN AND HEARTBEAT
      // TRACKING
      lastESPNowPacket = millis();
      peer->setLastSeen(millis());
      espNowFailsafeActive = false;
      pairingMode = true;
    }
  }
  return true;
}

// FIX: Update sendHeartbeat to check lastESPNowPacket first
void EspnowController::sendHeartbeat() {
  unsigned long now = millis();

  if (now - lastESPNowPacket < HEARTBEAT_INTERVAL) {
    return;
  }

  lastHeartbeatSend = now;
  lastESPNowPacket = now;

  JsonDocument doc;

  doc["type"] = CMD_HEARTBEAT;
  doc["value"] = 0;

  auto controllers = peers.getActiveControllerPeers();

  if (!controllers.empty()) {
    sendJsonDocument(&controllers[0], doc);
  }
}
validateSelectedPeerState EspnowController::validateSelectedPeer(Peer* peer) {
  Serial.println("[SELECT] Entered validateSelectedPeer");
  if (peer == nullptr) {
    Serial.println("[SELECT] ERROR: peer is null");
    return PEER_ERROR;
  }

  if (peer->getIsActivePeer()) {
    return ALREADY_SELECTED;
  }
  if (!addPeer(peer)) {
    return PEER_ERROR;
  }
  if (esp_now_is_peer_exist(peer->getMac())) {
    Serial.println("Peer already registered with ESP-NOW");
    return PASS_CHECK;
  }
  return PASS_CHECK;
}
sendPairRequestState EspnowController::sendPairRequest(Peer* peer) {
  ESPNowCommand pairPacket = {};

  pairPacket.type = CMD_PAIR_REQUEST;

  memcpy(pairPacket.data, localMac, 6);
  if (peer == nullptr || peer->getMac() == nullptr) {
    Serial.println("PAIR: Invalid peer or MAC");
    return PAIR_ERROR;
  }

  Serial.print("PAIR destination MAC: ");
  Serial.println(macToString(peer->getMac()));

  esp_err_t result =
      esp_now_send(peer->getMac(), reinterpret_cast<uint8_t*>(&pairPacket),
                   sizeof(pairPacket));

  if (result != ESP_OK) {
    Serial.print("PAIR send failed: ");

    Serial.print(result);

    Serial.print(" / ");

    Serial.println(esp_err_to_name(result));
    Serial.print("free memory");
    Serial.println(ESP.getFreeHeap());
    return PAIR_ERROR;
  }

  peer->setRole(PeerRole::RECEIVER);

  Serial.print("PAIR request sent to: ");

  Serial.println(peer->getName());
  std::cout << peer << "\n";

  return PAIR_SUCCESS;
}

void EspnowController::resetDiscover() {
  // Reset receiver list
  Serial.println("resetDiscover");

  peers.removePeers();

  // Start discovery

  discoveryActive = true;

  discoveryStarted = millis();

  lastDiscoverySend = 0;

  receiverListNeedsRefresh = true;

  lastHeartbeatSend = millis();

  pairingModeStartedTime = millis();

  pairingMode = true;
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

  EspNowEvent event = {};

  event.type = type;

  if (mac != nullptr) {
    memcpy(event.mac, mac, 6);
  }

  if (data != nullptr && len > 0) {
    if (len > ESPNOW_EVENT_DATA_MAX) {
      len = ESPNOW_EVENT_DATA_MAX;
    }

    memcpy(event.data, data, len);

    event.len = len;
  }

  xQueueSend(espNowEventQueue, &event, 0);
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

  EspNowEvent event = {};

  event.type = ESP_NOW_EVENT_SEND;

  memcpy(event.mac, tx_info->des_addr, 6);

  event.sendStatus = status;

  xQueueSend(callbackInstance->espNowEventQueue, &event, 0);
}
