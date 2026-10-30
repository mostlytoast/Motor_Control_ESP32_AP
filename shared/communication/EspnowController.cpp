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
    GetStateFromJsonCallback getStateFromJsonCallback_, bool wifi) {
  getJsonFromStateCallback = getJsonFromStateCallback_;
  getStateFromJsonCallback = getStateFromJsonCallback_;

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

  char macText[20];

  formatMac(localMac, macText, sizeof(macText));

  Serial.print("ESP-NOW MAC: ");

  Serial.println(macText);

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

  // Limit work per update so ESP-NOW processing does
  // not monopolize the main loop.

  while (processed < 8 &&
         xQueueReceive(espNowEventQueue, &event, 0) == pdTRUE) {
    processed++;
    // Peer peer = Peer(event.mac, WIFI_CHANNEL, "");
    // TODO MAJOR have to decide if we want to pass peer or mac through?
    // =================================================
    // RECEIVE
    // =================================================

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
    }

    // =================================================
    // SEND CALLBACK EVENT
    // =================================================

    else if (event.type == ESP_NOW_EVENT_SEND) {
      // SEND FAILED
      Peer* peer = peers.findPeer(event.mac);

      if (event.sendStatus == ESP_NOW_SEND_FAIL && peer != nullptr) {
        Serial.printf("ESP-NOW send failed -> %s", macToString(event.mac));
        // Controller-side receiver
        // Todo put into a controller function
        peer->setLastSeen(millis());
        if (peer->getIsActivePeerReceiver()) {
          peer->setIsPaired(false);
        }
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
    }
  }
}

// PAIRING MODE TIMEOUT

void EspnowController::ESPNowUpdatePairingMode() {
  if (pairingMode && pairingModeStartedTime != 0 &&
      millis() - pairingModeStartedTime > DEFAULT_PAIRING_WINDOW_MS) {
    pairingMode = false;

    Serial.println("Pairing window expired");
  }
}

// FAILSAFE

void EspnowController::ESPNowCheckFailsafe() {
  for (Peer& peer : peers.getActivePeers()) {
    if (peer.getLastSeen() > ESPNOW_TIMEOUT) {
      peers.removePeer(&peer);

      // resetDiscover();

      //   // Serial.println("ESP-NOW TIMEOUT - MOTOR STOP");
      //   // TODO remove this app. refs?
      //   // app.setTargetSpeed(0);

      //   // app.setAutoState(false);

      //   // app.setAutoMode(AUTO_IDLE);

      // espNowFailsafeActive = true;
    }
  }
  if (peers.getActiveControllerPeers().size() == 0) {
    espNowFailsafeActive = true;
  }
}

void EspnowController::processStateChanges() {
  for (const Peer& peer : peers.getActivePeers()) {
    sendServerState(peer);
  }
}

bool EspnowController::sendServerState(const Peer& peer) {
  JsonDocument fullState;

  getJsonFromStateCallback(fullState);

  // First call: establish the baseline
  if (!previousStateUpdated) {
    previousState = fullState;
    previousStateUpdated = true;
    return true;
  }

  // Nothing changed
  if (previousState == fullState) {
    return false;
  }

  // Something changed, update the baseline
  previousState = fullState;

  JsonDocument state;

  getJsonFromStateCallback(state);

  if (state.isNull()) {
    Serial.println("sendServerState: empty state");

    return false;
  }
  // Todo maybe want to have type in a doc wrapper around the orig json (makes
  // this more abstract)
  state["type"] = CMD_SERVER_STATE;

  return sendJsonDocument(&peer, state);
}

bool EspnowController::hasPreviousStateChanged() {
  JsonDocument fullState;

  getJsonFromStateCallback(fullState);

  // First call: establish the baseline
  if (!previousStateUpdated) {
    previousState = fullState;
    previousStateUpdated = true;
    return true;
  }

  // Nothing changed
  if (previousState == fullState) {
    return false;
  }

  // Something changed, update the baseline
  previousState = fullState;

  return true;
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
    // update with new info
    // Serial.println("set role");
    // std::cout << "peer " << *peer << " storedPeer " << *storedPeer;

    storedPeer->setRole(peer->getRole());
    // Serial.println("set role 2");
    // std::cout << "peer " << *peer << " storedPeer " << *storedPeer;
  }

  Serial.print("Local peer count: ");
  Serial.println(peers.getNumberOfPeers());

  // Now ensure the peer exists in ESP-NOW.
  if (esp_now_is_peer_exist(storedPeer->getMac())) {
    Serial.println("Peer already registered with ESP-NOW");
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
  receiverPeer.setIsValid(true);
  char macText[20];

  formatMac(receiverPeer.getMac(), macText, sizeof(macText));

  Serial.print("DISCOVERED RECEIVER: ");
  Serial.print(receiverPeer.getName());
  Serial.print(" / ");
  Serial.println(macText);

  if (!addPeer(&receiverPeer)) {
    Serial.print("failed to add receiver");
    receiverPeer.setIsValid(false);
    return false;
  }

  return true;
}

// PAIR CONTROLLER

bool EspnowController::pairController(const uint8_t* mac, const char* name) {
  // TODO condense to be shorter
  if (mac == nullptr) {
    return false;
  }
  Peer* peer = peers.findPeer(mac);

  if (peer != nullptr) {
    peer->setLastSeen(millis());

    if (name != nullptr && name[0] != '\0') {
      peer->setName(name);
    }

    addPeer(peer);

    return true;
  }

  Peer controllerPeer = Peer(mac, WIFI_CHANNEL, name);
  controllerPeer.setRole(PeerRole::CONTROLLER);
  controllerPeer.setLastSeen(millis());
  controllerPeer.setIsValid(true);

  if (!addPeer(&controllerPeer)) {
    controllerPeer.setIsValid(false);

    return false;
  }

  Serial.print("Controller paired: ");

  char macText[20];

  formatMac(mac, macText, sizeof(macText));

  Serial.println(macText);

  return true;
}

// CLAIM ACTIVE CONTROLLER

bool EspnowController::claimPeer(Peer* peer) {
  peer->setIsPaired(true);
  peer->setIsUsed(true);
  peer->setIsValid(true);
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
    if (peer.getIsValid()) {
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

  Serial.print("Discovery response -> ");
  Serial.print(macToString(mac));
  Serial.print(" result=");
  Serial.println(result);
}

// BINARY PAIR RESPONSE

void EspnowController::sendPairResponse(const uint8_t* mac, bool paired) {
  PairResponse response = {};

  response.type = CMD_PAIR_RESPONSE;

  response.value = paired ? 1 : 0;

  uint8_t receiverMac[6];

  WiFi.macAddress(receiverMac);

  memcpy(response.receiverMac, receiverMac, 6);

  response.paired = paired ? 1 : 0;

  strncpy(response.receiverName, RECEIVER_NAME,
          sizeof(response.receiverName) - 1);

  addPeer(mac, WIFI_CHANNEL, response.receiverName);

  esp_now_send(mac, reinterpret_cast<uint8_t*>(&response), sizeof(response));
}

// BINARY CONTROLLER STATUS

void EspnowController::sendControllerStatus(Peer* peer) {
  ESPNowCommand response = {};

  response.type = CMD_CONTROLLER_STATUS;

  response.value = -1;

  if (peer->getIsValid() && peer->getIsActivePeer() &&
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
      // Get the current packet
      uint8_t* packet = packets + (i * JSON_FRAGMENT_PACKET_SIZE);

      // Read its header
      JsonFragmentHeader header;
      memcpy(&header, packet, sizeof(header));

      // Send or process the packet here
      esp_err_t result =
          esp_now_send(destination, packet, JSON_FRAGMENT_PACKET_SIZE);
      if (result != ESP_OK) {
        Serial.print("JSON fragment send failed: ");

        Serial.print(result);

        Serial.print(" / ");

        Serial.println(esp_err_to_name(result));
        free(packets);

        return false;
      }
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
    Serial.println("DISCOVERY REQUEST RECEIVED");

    sendDiscoveryPacket(mac);

    return true;
  }

  // Pairing packets require enough room

  // TODO have this structured so all commands comming in are of espNow command
  // size?

  if (len != sizeof(ESPNowCommand)) {
    return false;
  }

  ESPNowCommand command = {};

  memcpy(&command, data, sizeof(command));

  // PAIR REQUEST

  if (command.type == CMD_PAIR_REQUEST) {
    Serial.println("PAIR REQUEST RECEIVED");

    if (!pairingMode) {
      sendPairResponse(mac, false);

      return true;
    }

    char controllerName[33];

    memcpy(controllerName, command.data, 32);

    controllerName[32] = '\0';

    sendPairResponse(mac, pairController(mac, controllerName));

    return true;
  }

  // UNPAIR

  Peer* peer = peers.findPeer(mac);

  if (command.type == CMD_UNPAIR_REQUEST) {
    peers.removePeer(peer->getMac());

    return true;
  }

  // CLAIM CONTROLLER

  if (command.type == CMD_CONTROLLER_CLAIM) {
    Serial.println("CMD_CONTROLLER_CLAIM RECEIVED");
    Serial.println(macToString(mac));

    if (peer->getIsPaired()) {
      Serial.println("Claim accepted");

      claimPeer(peer);
      sendControllerStatus(peer);
      if (peer != nullptr) {
        sendServerState(*peer);
      }
    } else {
      Serial.println("Claim rejected: controller not paired");
    }

    return true;
  }

  // STATUS

  if (command.type == CMD_CONTROLLER_STATUS) {
    if (peer->getIsPaired()) {
      sendControllerStatus(peer);
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

  char macText[20];
  formatMac(peer->getMac(), macText, sizeof(macText));

  Serial.print("CLAIM destination MAC: ");
  Serial.println(macText);
  if (!peer->getIsUsed() || !(peer->getRole() == PeerRole::RECEIVER) ) {
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

  // DISCOVERY RESPONSE

  if (len == sizeof(DiscoveryPacket)) {
    DiscoveryPacket packet = {};

    memcpy(&packet, data, sizeof(packet));

    if (packet.type == CMD_DISCOVERY_RESPONSE) {
      if (macEqual(packet.receiverMac, broadcastMac)) {
        // TODO check if this is always false figure out
        memcpy(packet.receiverMac, mac, 6);
      }

      packet.receiverName[sizeof(packet.receiverName) - 1] = '\0';

      if (addOrUpdateReceiver(&packet)) {
        receiverListNeedsRefresh = true;
      }

      return true;
    }
  }

  // Pair response

  if (len == sizeof(PairResponse)) {
    PairResponse response = {};
    memcpy(&response, data, sizeof(response));

    if (response.type == CMD_PAIR_RESPONSE) {
      Peer* peer = peers.findPeer(mac);

      if (peer != nullptr) {
        peer->setIsPaired(response.paired != 0);

        peer->setLastSeen(millis());

        receiverListNeedsRefresh = true;

        Serial.print("Receiver pairing: ");

        Serial.println((peer->getRole() == PeerRole::RECEIVER) ? "SUCCESS" : "FAILED");
        if (!sendControllerClaim(peer)) {
          Serial.println("Failed to send controller claim");
        }
      }

      return true;
    }
  }

  // Controller status

  if (len == sizeof(ESPNowCommand)) {
    ESPNowCommand command = {};

    memcpy(&command, data, sizeof(command));

    if (command.type == CMD_CONTROLLER_STATUS) {
      // TODO have to respond somehow?

      Serial.println("CONTROLLER STATUS RECEIVED");

      return true;
    }
  }

  return false;
}
// UNIFIED RECEIVE EVENT PROCESSING

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

    if (peer != nullptr) {
      peer->setLastSeen(millis());
    }
    // TODO get rid of assembly have jsonFragmentToJson directly return uint8_t
    getStateFromJsonCallback(doc);
  }

  else {
    Peer* peer = peers.findPeer(mac);
    if (peer != nullptr && peer->getIsActivePeer()) {
      // Record communication
      unsigned long now = (millis());

      lastESPNowPacket = now;

      peer->setLastSeen(now);

      espNowFailsafeActive = false;
      pairingMode = true;
    }
  }
  return true;
}

// HEARTBEAT

void EspnowController::sendHeartbeat() {
  unsigned long now = millis();

  if (now - lastHeartbeatSend < HEARTBEAT_INTERVAL) {
    return;
  }

  lastHeartbeatSend = now;

  JsonDocument doc;

  doc["type"] = CMD_HEARTBEAT;
  doc["value"] = 0;
  // TODO make loop through controllers only
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

  Serial.println("[SELECT] Peer pointer is valid");

  char macText[20];
  formatMac(peer->getMac(), macText, sizeof(macText));
  Serial.print("[SELECT] MAC before copy: ");
  Serial.println(macText);

  if (peer->getIsActivePeer()) {
    Serial.println("[SELECT] Already active");
    return ALREADY_SELECTED;
  }

  Serial.println("[SELECT] Copying selected peer");

  Peer selectedPeer = *peer;

  Serial.println("[SELECT] Removing old peers");
  peers.removePeers();

  Serial.print("[SELECT] Count after removal: ");
  Serial.println(peers.getNumberOfPeers());

  espNowFailsafeActive = true;

  Serial.println("[SELECT] Re-adding selected peer");

  Peer* storedPeer = peers.addPeer(&selectedPeer);

  if (storedPeer == nullptr) {
    Serial.println("[SELECT] ERROR: addPeer failed");
    return PEER_ERROR;
  }

  formatMac(storedPeer->getMac(), macText, sizeof(macText));
  Serial.print("[SELECT] MAC after re-add: ");
  Serial.println(macText);

  Serial.println("[SELECT] Registering with ESP-NOW");

  if (!addPeer(storedPeer)) {
    Serial.println("[SELECT] ERROR: ESP-NOW registration failed");
    return PEER_ERROR;
  }

  Serial.println("[SELECT] Selection validation passed");
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

  char macText[20];
  formatMac(peer->getMac(), macText, sizeof(macText));

  Serial.print("PAIR destination MAC: ");
  Serial.println(macText);
  esp_err_t result =
      esp_now_send(peer->getMac(), reinterpret_cast<uint8_t*>(&pairPacket),
                   sizeof(pairPacket));

  if (result != ESP_OK) {
    Serial.print("PAIR send failed: ");

    Serial.print(result);

    Serial.print(" / ");

    Serial.println(esp_err_to_name(result));

    return PAIR_ERROR;
  }
  
  peer->setRole(PeerRole::RECEIVER);

  Serial.print("PAIR request sent to: ");

  Serial.println(peer->getName());

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
  Serial.print("number peers (called)");
  Serial.println(peers.getNumberOfPeers());
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
  Serial.print("number of active receiver peers ");
  Serial.println(peers.getActiveReceiverPeers().size());
  return peers.getActiveReceiverPeers();
}

std::vector<Peer> EspnowController::getPeers() {
  Serial.print("number of peers ");
  Serial.println(peers.getPeers().size());
  return peers.getPeers();
}

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
