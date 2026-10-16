#include "EspnowController.h"

EspnowController espnowController;
EspnowController* EspnowController::callbackInstance = nullptr;
namespace {
JsonDocument previousState;
bool previousStateUpdated = false;
}  // namespace
// =====================================================
// PEER MANAGEMENT
// =====================================================

// TODO unify names of peer receiver and controller to be peer?? maybe not
bool EspnowController::addPeer(const uint8_t* mac, uint8_t channel) {
  if (mac == nullptr) {
    return false;
  }

  if (esp_now_is_peer_exist(mac)) {
    return true;
  }

  esp_now_peer_info_t peerInfo = {};

  memcpy(peerInfo.peer_addr, mac, 6);

  peerInfo.channel = channel;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;

  esp_err_t result = esp_now_add_peer(&peerInfo);

  if (result == ESP_OK || result == ESP_ERR_ESPNOW_EXIST) {
    Serial.println("ESP-NOW peer added");
    return true;
  }

  Serial.print("EeSP-NOW add peer failed: ");
  Serial.print(result);
  Serial.print(" / ");
  Serial.println(esp_err_to_name(result));

  return false;
}

void EspnowController::removePeer(const uint8_t* mac) {
  if (mac == nullptr) {
    return;
  }

  if (esp_now_is_peer_exist(mac)) {
    esp_now_del_peer(mac);
  }
}

void EspnowController::removePeers() {
  for (int i = 0; i < receiverCount; i++) {
    if (receivers[i].used) {
      removePeer(receivers[i].mac);
    }
  }
}
// =====================================================
// EVENT QUEUE
// =====================================================

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

int EspnowController::addOrUpdateReceiver(const DiscoveryPacket* packet) {
  if (packet == nullptr) {
    return -1;
  }

  int existing = findController(packet->receiverMac);

  // ---------------------------------------------------
  // Existing receiver
  // ---------------------------------------------------

  if (existing >= 0) {
    ReceiverInfo& receiver = receivers[existing];

    memcpy(receiver.mac, packet->receiverMac, 6);

    strncpy(receiver.name, packet->receiverName, sizeof(receiver.name) - 1);

    receiver.name[sizeof(receiver.name) - 1] = '\0';

    receiver.channel = packet->channel;

    receiver.lastSeen = millis();

    return existing;
  }

  // ---------------------------------------------------
  // Receiver list full
  // ---------------------------------------------------

  if (receiverCount >= MAX_RECEIVERS) {
    Serial.println("Receiver list full");

    return -1;
  }

  // ---------------------------------------------------
  // New receiver
  // ---------------------------------------------------

  int index = receiverCount++;

  ReceiverInfo& receiver = receivers[index];

  memset(&receiver, 0, sizeof(receiver));

  receiver.used = true;

  memcpy(receiver.mac, packet->receiverMac, 6);

  strncpy(receiver.name, packet->receiverName, sizeof(receiver.name) - 1);

  receiver.name[sizeof(receiver.name) - 1] = '\0';

  receiver.channel = packet->channel;

  receiver.paired = false;

  receiver.lastSeen = millis();

  char macText[20];

  formatMac(receiver.mac, macText, sizeof(macText));

  Serial.print("DISCOVERED RECEIVER: ");
  Serial.print(receiver.name);
  Serial.print(" / ");
  Serial.println(macText);

  addPeer(receiver.mac, receiver.channel);

  return index;
}

// =====================================================
// RECEIVER-SIDE CONTROLLER MANAGEMENT
// =====================================================

int EspnowController::findController(const uint8_t* mac) {
  if (mac == nullptr) {
    return -1;
  }

  for (int i = 0; i < receiverCount; i++) {
    if (!receivers[i].used) {
      continue;
    }

    if (macEqual(receivers[i].mac, mac)) {
      return i;
    }
  }

  return -1;
}

bool EspnowController::isPairedController(const uint8_t* mac) {
  return findController(mac) >= 0;
}

int EspnowController::findFreeControllerSlot() {
  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (!controllers[i].valid) {
      return i;
    }
  }

  return -1;
}

// =====================================================
// PAIR CONTROLLER
// =====================================================

int EspnowController::pairController(const uint8_t* mac,
                                     const char* controllerName) {
  if (mac == nullptr) {
    return -1;
  }

  int existing = findController(mac);

  // ---------------------------------------------------
  // Already paired
  // ---------------------------------------------------

  if (existing >= 0) {
    controllers[existing].lastSeen = millis();

    if (controllerName != nullptr && controllerName[0] != '\0') {
      strncpy(controllers[existing].name, controllerName,
              sizeof(controllers[existing].name) - 1);

      controllers[existing].name[sizeof(controllers[existing].name) - 1] = '\0';
    }

    addPeer(mac, WIFI_CHANNEL);

    // saveControllers();

    return existing;
  }

  // ---------------------------------------------------
  // Find slot
  // ---------------------------------------------------

  int slot = findFreeControllerSlot();

  if (slot < 0) {
    return -1;
  }

  memset(&controllers[slot], 0, sizeof(controllers[slot]));

  controllers[slot].valid = true;

  memcpy(controllers[slot].mac, mac, 6);

  if (controllerName != nullptr && controllerName[0] != '\0') {
    strncpy(controllers[slot].name, controllerName,
            sizeof(controllers[slot].name) - 1);

  } else {
    snprintf(controllers[slot].name, sizeof(controllers[slot].name),
             "Controller %d", slot + 1);
  }

  controllers[slot].lastSeen = millis();

  if (!addPeer(mac, WIFI_CHANNEL)) {
    controllers[slot].valid = false;

    return -1;
  }

  // saveControllers();

  Serial.print("Controller paired: ");

  char macText[20];

  formatMac(mac, macText, sizeof(macText));

  Serial.println(macText);

  return slot;
}

// =====================================================
// REMOVE CONTROLLER
// =====================================================

bool EspnowController::removeController(int slot) {
  if (slot < 0 || slot >= MAX_CONTROLLERS) {
    return false;
  }

  if (!controllers[slot].valid) {
    return false;
  }

  uint8_t mac[6];

  memcpy(mac, controllers[slot].mac, 6);

  removePeer(mac);

  if (activeController == slot) {
    activeController = -1;

    lastActiveControllerPacket = 0;
    // TODO want to remove app. refs
    app.setTargetSpeed(0);

    app.setUserSpeed(0);

    app.setAutoState(false);

    app.setAutoMode(AUTO_IDLE);
  }

  memset(&controllers[slot], 0, sizeof(controllers[slot]));

  // saveControllers();

  Serial.print("Controller removed: ");

  char macText[20];

  formatMac(mac, macText, sizeof(macText));

  Serial.println(macText);

  return true;
}

// =====================================================
// CLAIM ACTIVE CONTROLLER
// =====================================================

bool EspnowController::claimController(const uint8_t* mac) {
  Serial.println("controller claimed1");
  int slot = findController(mac);

  if (slot < 0) {
    return false;
  }
  Serial.println("controller claimed2");

  activeController = slot;

  lastActiveControllerPacket = millis();

  controllers[slot].lastSeen = millis();

  Serial.print("ACTIVE CONTROLLER -> ");

  Serial.println(controllers[slot].name);

  return true;
}

bool EspnowController::isActiveController(const uint8_t* mac) {
  int slot = findController(mac);

  if (slot < 0) {
    return false;
  }

  return slot == activeController;
}

// =====================================================
// BINARY DISCOVERY RESPONSE
// =====================================================

void EspnowController::sendDiscoveryPacket(const uint8_t* destination) {
  DiscoveryPacket response = {};

  response.type = CMD_DISCOVERY_RESPONSE;

  response.value = MAX_CONTROLLERS;

  uint8_t receiverMac[6];

  WiFi.macAddress(receiverMac);

  memcpy(response.receiverMac, receiverMac, 6);

  uint8_t count = 0;

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (controllers[i].valid) {
      count++;
    }
  }

  response.pairedCount = count;

  response.pairingMode = pairingMode ? 1 : 0;

  strncpy(response.receiverName, RECEIVER_NAME,
          sizeof(response.receiverName) - 1);

  response.channel = WIFI_CHANNEL;

  if (!isBroadcastMac(destination)) {
    addPeer(destination, WIFI_CHANNEL);
  }

  esp_err_t result = esp_now_send(
      destination, reinterpret_cast<uint8_t*>(&response), sizeof(response));

  Serial.print("Discovery response -> ");
  Serial.print(macToString(destination));
  Serial.print(" result=");
  Serial.println(result);
}

// =====================================================
// BINARY PAIR RESPONSE
// =====================================================

void EspnowController::sendPairResponse(const uint8_t* destination, int slot,
                                        bool paired) {
  PairResponse response = {};

  response.type = CMD_PAIR_RESPONSE;

  response.value = paired ? 1 : 0;

  uint8_t receiverMac[6];

  WiFi.macAddress(receiverMac);

  memcpy(response.receiverMac, receiverMac, 6);
  // todo should 255 be max_controllers?
  response.controllerSlot = slot >= 0 ? slot : 255;

  response.paired = paired ? 1 : 0;

  strncpy(response.receiverName, RECEIVER_NAME,
          sizeof(response.receiverName) - 1);

  addPeer(destination, WIFI_CHANNEL);

  esp_now_send(destination, reinterpret_cast<uint8_t*>(&response),
               sizeof(response));
}

// =====================================================
// BINARY CONTROLLER STATUS
// =====================================================

void EspnowController::sendControllerStatus(const uint8_t* destination) {
  ESPNowCommand response = {};

  response.type = CMD_CONTROLLER_STATUS;

  response.value = activeController;

  if (activeController >= 0 && activeController < MAX_CONTROLLERS &&
      controllers[activeController].valid) {
    memcpy(response.data, controllers[activeController].mac, 6);
  }

  addPeer(destination, WIFI_CHANNEL);

  esp_now_send(destination, reinterpret_cast<uint8_t*>(&response),
               sizeof(response));
}

// =====================================================
// GENERIC JSON SEND
//
// Automatically fragments any JSON document into
// packets <= 250 bytes.
//
// The receiver reconstructs the original JSON before
// deserializing it.
// =====================================================

bool EspnowController::sendJsonDocument(const uint8_t* destination,
                                        JsonDocument& doc) {
  if (destination == nullptr) {
    return false;
  }

  // ---------------------------------------------------
  // Make sure peer exists
  // ---------------------------------------------------

  if (!esp_now_is_peer_exist(destination)) {
    if (!addPeer(destination, WIFI_CHANNEL)) {
      return false;
    }
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
  if (jsonAssembler.disassembleJson(destination, doc, packets, totalChunks)) {
    // TODO use object thats an array of packets
    // Read each packet
    for (size_t i = 0; i < totalChunks; i++) {
      // Get the current packet
      uint8_t* packet = packets + (i * JSON_FRAGMENT_PACKET_SIZE);
      // Serial.println("disassembled Json");

      // for (size_t j = 0; j < JSON_FRAGMENT_PACKET_SIZE; j++) {
      //   Serial.printf("%02X ", packet[j]);
      // }

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
// =====================================================
// JSON COMMAND
// =====================================================

bool EspnowController::sendCommand(uint8_t type, int16_t value) {
  if (activeReceiver < 0 || activeReceiver >= receiverCount) {
    return false;
  }

  ReceiverInfo& receiver = receivers[activeReceiver];

  if (!receiver.used || !receiver.paired) {
    return false;
  }

  JsonDocument doc;

  doc["type"] = type;
  doc["value"] = value;

  return sendJsonDocument(receiver.mac, doc);
}

bool EspnowController::hasPreviousStateChanged() {
  JsonDocument fullState;

  getJsonFromState(fullState);

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

// =====================================================
// SEND SERVER STATE
//
// IMPORTANT:
//
// This function ONLY sends state.
// It does not decide whether the state should be sent.
//
// Local changes should call:
//     notifyLocalStateChanged();
//
// Remote changes are marked as STATE_CHANGE_REMOTE
// and are therefore not sent again.
// =====================================================

bool EspnowController::sendServerState(const uint8_t* destination) {
  if (destination == nullptr) {
    return false;
  }

  if (!hasPreviousStateChanged()) {
    return false;
  }

  JsonDocument fullState;

  getJsonFromState(fullState);

  if (fullState.isNull()) {
    Serial.println("sendServerState: empty state");

    return false;
  }

  fullState["type"] = CMD_SERVER_STATE;

  return sendJsonDocument(destination, fullState);
}

// =====================================================
// PROCESS STATE CHANGES
//
// This is the ONLY normal path that sends changed state.
//
// LOCAL:
//     send state
//
// REMOTE:
//     do not send state
// =====================================================

void EspnowController::processStateChanges() {
  // Controller-side: send to the selected receiver.
  if (activeReceiver >= 0 && activeReceiver < receiverCount &&
      receivers[activeReceiver].used && receivers[activeReceiver].paired) {
    sendServerState(receivers[activeReceiver].mac);
    return;
  }

  // Receiver-side: send to the active controller.
  if (activeController >= 0 && activeController < MAX_CONTROLLERS &&
      controllers[activeController].valid) {
    sendServerState(controllers[activeController].mac);
  }
}

// =====================================================
// PROCESS JSON SERVER STATE
// =====================================================

void EspnowController::processServerState(JsonDocument& doc) {
  if (!doc["type"].is<uint8_t>() ||
      doc["type"].as<uint8_t>() != CMD_SERVER_STATE) {
    Serial.println("Invalid JSON server state type");

    return;
  }

  // ---------------------------------------------------
  // Merge state properties
  // ---------------------------------------------------

  JsonObjectConst incomingState = doc["state"].as<JsonObjectConst>();

  if (!incomingState.isNull()) {
    JsonObject storedState = serverState["state"].to<JsonObject>();

    for (JsonPairConst pair : incomingState) {
      storedState[pair.key()] = pair.value();
    }
  }

  // ---------------------------------------------------
  // Tracks
  // ---------------------------------------------------

  JsonArrayConst incomingTracks = doc["tracks"].as<JsonArrayConst>();

  if (!incomingTracks.isNull()) {
    serverState["tracks"] = incomingTracks;
  }

  serverState["type"] = CMD_SERVER_STATE;

  // todo have this be separate want espnow to be on its own
  getStateFromJson(doc);
}

// =====================================================
// PROCESS JSON COMMAND ON RECEIVER
// =====================================================

void EspnowController::processJsonCommand(const uint8_t* sourceMac,
                                          JsonDocument& doc) {
  if (sourceMac == nullptr) {
    return;
  }

  if (!doc["type"].is<uint8_t>()) {
    return;
  }

  uint8_t type = doc["type"].as<uint8_t>();

  int16_t value = 0;

  if (doc["value"].is<int>()) {
    value = doc["value"].as<int16_t>();
  }

  // ---------------------------------------------------
  // Controller must be paired
  // ---------------------------------------------------

  int controllerSlot = findController(sourceMac);

  if (controllerSlot < 0) {
    Serial.print("Rejected JSON from unpaired controller: ");

    Serial.println(macToString(sourceMac));

    return;
  }

  // ---------------------------------------------------
  // Initial active controller
  // ---------------------------------------------------

  if (activeController < 0) {
    Serial.println("set controller");

    activeController = controllerSlot;
    Serial.println(activeController);
  }

  // ---------------------------------------------------
  // Only active controller controls
  // ---------------------------------------------------

  if (controllerSlot != activeController) {
    return;
  }

  // ---------------------------------------------------
  // Record communication
  // ---------------------------------------------------

  unsigned long now = millis();

  lastESPNowPacket = now;

  lastActiveControllerPacket = now;

  controllers[controllerSlot].lastSeen = now;

  // ---------------------------------------------------
  // Convert JSON command to pending command
  // ---------------------------------------------------

  ESPNowCommand command = {};

  command.type = type;
  command.value = value;

  espNowPendingCommand = command;

  memcpy(pendingSourceMac, sourceMac, 6);

  espNowCommandPending = true;

  // ---------------------------------------------------
  // Clear failsafe
  // ---------------------------------------------------

  bool wasFailsafe = espNowFailsafeActive;

  espNowFailsafeActive = false;
}

// PROCESS BINARY RECEIVER-SIDE PACKETS

bool EspnowController::processReceiverBinaryPacket(const uint8_t* sourceMac,
                                                   const uint8_t* data,
                                                   int len) {
  if (sourceMac == nullptr || data == nullptr || len <= 0) {
    return false;
  }

  // ---------------------------------------------------
  // DISCOVERY
  // ---------------------------------------------------

  if (len >= 1 && data[0] == CMD_DISCOVER) {
    Serial.println("DISCOVERY REQUEST RECEIVED");

    sendDiscoveryPacket(sourceMac);

    return true;
  }

  // ---------------------------------------------------
  // Pairing packets require enough room
  // ---------------------------------------------------

  if (len != sizeof(ESPNowCommand)) {
    return false;
  }

  ESPNowCommand command = {};

  memcpy(&command, data, sizeof(command));

  // ---------------------------------------------------
  // PAIR REQUEST
  // ---------------------------------------------------

  if (command.type == CMD_PAIR_REQUEST) {
    Serial.println("PAIR REQUEST RECEIVED");

    if (!pairingMode) {
      sendPairResponse(sourceMac, -1, false);

      return true;
    }

    char controllerName[33];

    memcpy(controllerName, command.data, 32);

    controllerName[32] = '\0';

    int slot = pairController(sourceMac, controllerName);

    sendPairResponse(sourceMac, slot, slot >= 0);

    return true;
  }

  // ---------------------------------------------------
  // UNPAIR
  // ---------------------------------------------------

  if (command.type == CMD_UNPAIR_REQUEST) {
    int slot = findController(sourceMac);

    if (slot >= 0) {
      removeController(slot);
    }

    return true;
  }

  // ---------------------------------------------------
  // CLAIM CONTROLLER
  // ---------------------------------------------------

  if (command.type == CMD_CONTROLLER_CLAIM) {
    Serial.println("CMD_CONTROLLER_CLAIM RECEIVED");
    Serial.println(macToString(sourceMac));

    if (isPairedController(sourceMac)) {
      Serial.println("Claim accepted");

      claimController(sourceMac);
      sendControllerStatus(sourceMac);
      sendServerState(sourceMac);
    } else {
      Serial.println("Claim rejected: controller not paired");
    }

    return true;
  }

  // ---------------------------------------------------
  // STATUS
  // ---------------------------------------------------

  if (command.type == CMD_CONTROLLER_STATUS) {
    if (isPairedController(sourceMac)) {
      sendControllerStatus(sourceMac);
    }

    return true;
  }

  return false;
}

bool EspnowController::sendControllerClaim(int receiverIndex) {
  if (receiverIndex < 0 || receiverIndex >= receiverCount) {
    Serial.println("CLAIM: Invalid receiver index");
    return false;
  }

  ReceiverInfo& receiver = receivers[receiverIndex];

  if (!receiver.used || !receiver.paired) {
    Serial.println("CLAIM: Receiver is not paired");
    return false;
  }

  if (!esp_now_is_peer_exist(receiver.mac)) {
    if (!addPeer(receiver.mac, receiver.channel)) {
      Serial.println("CLAIM: Failed to add receiver peer");
      return false;
    }
  }

  ESPNowCommand command = {};
  command.type = CMD_CONTROLLER_CLAIM;
  command.value = 0;

  esp_err_t result =
      esp_now_send(receiver.mac, reinterpret_cast<const uint8_t*>(&command),
                   sizeof(command));

  Serial.print("Sending CMD_CONTROLLER_CLAIM to ");
  Serial.print(receiver.name);
  Serial.print(": ");
  Serial.println(esp_err_to_name(result));

  return result == ESP_OK;
}

// =====================================================
// PROCESS CONTROLLER-SIDE BINARY PACKETS
// =====================================================

bool EspnowController::processControllerBinaryPacket(const uint8_t* sourceMac,
                                                     const uint8_t* data,
                                                     int len) {
  if (sourceMac == nullptr || data == nullptr || len <= 0) {
    return false;
  }

  // ---------------------------------------------------
  // DISCOVERY RESPONSE
  // ---------------------------------------------------

  if (len == sizeof(DiscoveryPacket)) {
    DiscoveryPacket packet = {};

    memcpy(&packet, data, sizeof(packet));

    if (packet.type == CMD_DISCOVERY_RESPONSE) {
      if (macEqual(packet.receiverMac, broadcastMac)) {
        memcpy(packet.receiverMac, sourceMac, 6);
      }

      packet.receiverName[sizeof(packet.receiverName) - 1] = '\0';

      int index = addOrUpdateReceiver(&packet);

      if (index >= 0) {
        receiverListNeedsRefresh = true;
      }

      return true;
    }
  }

  // ---------------------------------------------------
  // Pair response
  // ---------------------------------------------------

  if (len == sizeof(PairResponse)) {
    PairResponse response = {};
    memcpy(&response, data, sizeof(response));

    if (response.type != CMD_PAIR_RESPONSE) {
      return false;
    }

    // Find the receiver by MAC.
    // Do NOT use findController(), which searches
    // the receiver-side controllers[] array.
    int index = -1;

    for (int i = 0; i < receiverCount; i++) {
      if (receivers[i].used && macEqual(receivers[i].mac, sourceMac)) {
        index = i;
        break;
      }
    }

    if (index < 0) {
      Serial.println("PAIR RESPONSE: Unknown receiver");
      return true;
    }

    receivers[index].paired = response.paired != 0;
    receivers[index].lastSeen = millis();
    receiverListNeedsRefresh = true;

    if (!receivers[index].paired) {
      Serial.println("Pairing failed");
      return true;
    }

    Serial.print("Pairing successful with ");
    Serial.println(receivers[index].name);

    // Make this the active receiver.
    activeReceiver = index;

    // Send the claim immediately after pairing.
    if (!sendControllerClaim(index)) {
      Serial.println("Failed to send controller claim");
    }

    return true;
  }

  // ---------------------------------------------------
  // Controller status
  // ---------------------------------------------------

  if (len == sizeof(ESPNowCommand)) {
    ESPNowCommand command = {};

    memcpy(&command, data, sizeof(command));

    if (command.type == CMD_CONTROLLER_STATUS) {
      Serial.println("CONTROLLER STATUS RECEIVED");

      return true;
    }
  }

  return false;
}
// =====================================================
// UNIFIED RECEIVE EVENT PROCESSING
// =====================================================

bool EspnowController::processJsonFragment(const uint8_t* sourceMac,
                                           const uint8_t* data, size_t len) {
  JsonDocument doc;
  JsonReassembly assembly;
  if (!jsonAssembler.jsonFragmentToJson(sourceMac, data, len, doc)) {
    return false;
  }
  // uint8_t buffer;
  // size_t length;
  // jsonAssembler.jsonFragmentToJson(sourceMac, data, len, doc, buffer,
  // length);

  uint8_t type = doc["type"].as<uint8_t>();
  // Serial.println("reassembled json");
  // serializeJsonPretty(doc, Serial);
  // ---------------------------------------------------
  // SERVER STATE
  // ---------------------------------------------------

  if (type == CMD_SERVER_STATE) {
    int receiverIndex = findController(sourceMac);

    if (receiverIndex >= 0) {
      receivers[receiverIndex].lastSeen = millis();
    }
    // TODO get rid of assembly have jsonFragmentToJson directly return uint8_t
    // processServerState(data, len);
    processServerState(doc);
  }

  // ---------------------------------------------------
  // Other JSON commands
  // ---------------------------------------------------

  else {
    processJsonCommand(sourceMac, doc);
  }
  return true;
}
// =====================================================
// UNIFIED RECEIVE EVENT PROCESSING
// =====================================================

void EspnowController::processESPNowEvents() {
  if (espNowEventQueue == nullptr) {
    return;
  }

  EspNowEvent event;

  int processed = 0;

  // ---------------------------------------------------
  // Limit work per update so ESP-NOW processing does
  // not monopolize the main loop.
  // ---------------------------------------------------

  while (processed < 8 &&
         xQueueReceive(espNowEventQueue, &event, 0) == pdTRUE) {
    processed++;

    // =================================================
    // RECEIVE EVENT
    // =================================================

    if (event.type == ESP_NOW_EVENT_RECEIVE) {
      if (event.len <= 0) {
        continue;
      }

      // ------------------------------------------------
      // 1. Try receiver-side binary packets
      // ------------------------------------------------

      if (processReceiverBinaryPacket(event.mac, event.data, event.len)) {
        continue;
      }

      // ------------------------------------------------
      // 2. Try controller-side binary packets
      // ------------------------------------------------

      if (processControllerBinaryPacket(event.mac, event.data, event.len)) {
        continue;
      }

      // ------------------------------------------------
      // 3. Try generic JSON fragmentation
      //
      // processJsonFragment() handles:
      //
      //   - identifying JSON fragments
      //   - creating reassembly buffers
      //   - out-of-order packets
      //   - duplicate packets
      //   - reconstructing the JSON
      //   - deserializing the complete JSON
      //   - dispatching CMD_SERVER_STATE
      //   - dispatching normal JSON commands
      //
      // IMPORTANT:
      //
      // We do NOT call deserializeJson() here.
      // Individual ESP-NOW packets are fragments and
      // therefore are not necessarily valid JSON.
      // ------------------------------------------------

      if (processJsonFragment(event.mac, event.data, event.len)) {
        continue;
      }

      // ------------------------------------------------
      // 4. Unknown packet
      // ------------------------------------------------

      Serial.print("Unknown ESP-NOW packet from ");

      Serial.print(macToString(event.mac));

      Serial.print(" (");

      Serial.print(event.len);

      Serial.println(" bytes)");
    }

    // =================================================
    // SEND CALLBACK EVENT
    // =================================================

    else if (event.type == ESP_NOW_EVENT_SEND) {
      // ------------------------------------------------
      // SEND FAILED
      // ------------------------------------------------

      if (event.sendStatus == ESP_NOW_SEND_FAIL) {
        Serial.print("ESP-NOW send failed -> ");

        Serial.println(macToString(event.mac));

        // ------------------------------------------------
        // Controller-side receiver
        // ------------------------------------------------

        int index = findController(event.mac);

        if (index >= 0) {
          receivers[index].lastSeen = millis();

          // ----------------------------------------------
          // A failed send to the active receiver means
          // the connection may no longer be valid.
          // ----------------------------------------------

          if (index == activeReceiver) {
            receivers[index].paired = false;
          }
        }

        // ------------------------------------------------
        // Receiver-side controller
        //
        // Nothing else is required here. The controller
        // remains paired until explicitly removed.
        // ------------------------------------------------
      }

      // ------------------------------------------------
      // SEND SUCCESS
      // ------------------------------------------------

      else if (event.sendStatus == ESP_NOW_SEND_SUCCESS) {
        int index = findController(event.mac);

        if (index >= 0) {
          receivers[index].lastSeen = millis();
        }
      }
    }
  }
}
// =====================================================
// SEND PAIR REQUEST
// =====================================================

bool EspnowController::pairWithReceiver(int index) {
  if (index < 0 || index >= receiverCount || !receivers[index].used) {
    return false;
  }

  // ---------------------------------------------------
  // Already paired
  // ---------------------------------------------------

  if (index == activeReceiver && receivers[index].paired) {
    return true;
  }

  // ---------------------------------------------------
  // Peer
  // ---------------------------------------------------

  if (!addPeer(receivers[index].mac, receivers[index].channel)) {
    return false;
  }

  activeReceiver = index;

  receivers[index].paired = false;

  serverState.clear();

  // ---------------------------------------------------
  // Binary pairing packet
  // ---------------------------------------------------

  PairPacket pairPacket = {};

  pairPacket.type = CMD_PAIR_REQUEST;

  memcpy(pairPacket.receiverMac, localMac, 6);

  esp_err_t result =
      esp_now_send(receivers[index].mac,
                   reinterpret_cast<uint8_t*>(&pairPacket), sizeof(pairPacket));

  if (result != ESP_OK) {
    Serial.print("PAIR send failed: ");

    Serial.print(result);

    Serial.print(" / ");

    Serial.println(esp_err_to_name(result));

    receivers[index].paired = false;

    return false;
  }

  Serial.print("PAIR request sent to: ");

  Serial.println(receivers[index].name);

  return true;
}

// =====================================================
// SEND DISCOVERY
// =====================================================

void EspnowController::sendDiscovery() {
  ESPNowCommand command = {};

  command.type = CMD_DISCOVER;

  command.value = 0;

  esp_err_t result = esp_now_send(
      broadcastMac, reinterpret_cast<uint8_t*>(&command), sizeof(command));

  if (result != ESP_OK) {
    Serial.print("Discovery send failed: ");

    Serial.print(result);

    Serial.print(" / ");

    Serial.println(esp_err_to_name(result));
  }
}

// =====================================================
// DISCOVERY UPDATE
// =====================================================

void EspnowController::processDiscovery() {
  if (!discoveryActive) {
    return;
  }

  unsigned long now = millis();

  if (now - lastDiscoverySend >= DISCOVERY_INTERVAL) {
    lastDiscoverySend = now;

    sendDiscovery();
  }

  if (now - discoveryStarted >= DISCOVERY_TIMEOUT) {
    discoveryActive = false;

    receiverListNeedsRefresh = true;

    Serial.print("Discovery complete. Found ");

    Serial.print(receiverCount);

    Serial.println(" receiver(s)");
  }
}

// =====================================================
// FAILSAFE
// =====================================================

void EspnowController::ESPNow_checkFailsafe() {
  if (lastESPNowPacket == 0) {
    return;
  }

  unsigned long elapsed = millis() - lastESPNowPacket;

  if (elapsed > ESPNOW_TIMEOUT) {
    if (!espNowFailsafeActive) {
      // Serial.println("ESP-NOW TIMEOUT - MOTOR STOP");
      // TODO remove this app. refs
      app.setTargetSpeed(0);

      app.setAutoState(false);

      app.setAutoMode(AUTO_IDLE);

      espNowFailsafeActive = true;
    }
  }
}

// =====================================================
// PAIRING MODE TIMEOUT
// =====================================================

void EspnowController::ESPNow_updatePairingMode() {
  if (pairingMode && pairingModeStarted != 0 &&
      millis() - pairingModeStarted > DEFAULT_PAIRING_WINDOW_MS) {
    pairingMode = false;

    Serial.println("Pairing window expired");
  }
}

// =====================================================
// HEARTBEAT
// =====================================================

void EspnowController::sendHeartbeat() {
  unsigned long now = millis();

  if (now - lastHeartbeatSend < HEARTBEAT_INTERVAL) {
    return;
  }

  lastHeartbeatSend = now;

  sendCommand(CMD_HEARTBEAT, 0);
}

// =====================================================
// MAIN UPDATE
// =====================================================

void EspnowController::espnowUpdate() {
  // ---------------------------------------------------
  // Remove incomplete JSON messages that timed out
  // ---------------------------------------------------

  jsonAssembler.cleanupJsonReassemblies();

  // ---------------------------------------------------
  // Process queued RX packets
  // ---------------------------------------------------

  processESPNowEvents();

  // ---------------------------------------------------
  // Controller-side discovery
  // ---------------------------------------------------

  processDiscovery();

  // ---------------------------------------------------
  // Receiver-side pairing timeout
  // ---------------------------------------------------

  ESPNow_updatePairingMode();

  // ---------------------------------------------------
  // Receiver failsafe
  // ---------------------------------------------------

  ESPNow_checkFailsafe();

  // ---------------------------------------------------
  // Process local/remote state changes
  //
  // IMPORTANT:
  //
  // This replaces the old 200 ms state transmission.
  // ---------------------------------------------------

  processStateChanges();

  // ---------------------------------------------------
  // Controller-side heartbeat
  // ---------------------------------------------------

  if (activeReceiver >= 0 && activeReceiver < receiverCount &&
      receivers[activeReceiver].used && receivers[activeReceiver].paired) {
    sendHeartbeat();
  }
}

// =====================================================
// RECEIVE CALLBACK
//
// KEEP THIS EXTREMELY LIGHTWEIGHT.
// =====================================================

void EspnowController::onESPNowReceive(const esp_now_recv_info_t* info,
                                       const uint8_t* data, int len) {
  if (info == nullptr || data == nullptr || len <= 0 ||
      callbackInstance == nullptr) {
    return;
  }

  callbackInstance->queueEspNowEvent(ESP_NOW_EVENT_RECEIVE, info->src_addr,
                                     data, len);
}

// =====================================================
// SEND CALLBACK
// =====================================================

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

// =====================================================
// SETUP
// =====================================================

bool EspnowController::setupESPNow(bool wifi) {
  callbackInstance = this;
  Serial.println();
  Serial.println("================================");

  Serial.println("Initializing ESP-NOW");

  Serial.println("================================");

  // ---------------------------------------------------
  // WiFi
  // ---------------------------------------------------

  if (wifi) {
    WiFi.mode(WIFI_STA);

    delay(100);

    WiFi.disconnect();

    delay(100);
  }

  // ---------------------------------------------------
  // MAC
  // ---------------------------------------------------

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

  // ---------------------------------------------------
  // Channel
  // ---------------------------------------------------

  result = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (result != ESP_OK) {
    Serial.print("Failed to set WiFi channel: ");

    Serial.println(esp_err_to_name(result));

    return false;
  }

  // ---------------------------------------------------
  // ESP-NOW
  // ---------------------------------------------------

  result = esp_now_init();

  if (result != ESP_OK) {
    Serial.print("ESP-NOW initialization FAILED: ");

    Serial.println(esp_err_to_name(result));

    return false;
  }

  Serial.println("ESP-NOW initialized");

  // ---------------------------------------------------
  // Event queue
  // ---------------------------------------------------

  espNowEventQueue = xQueueCreate(ESPNOW_EVENT_QUEUE_SIZE, sizeof(EspNowEvent));

  if (espNowEventQueue == nullptr) {
    Serial.println("ERROR: Could not create ESP-NOW event queue");

    esp_now_deinit();

    return false;
  }

  // ---------------------------------------------------
  // Receive callback
  // ---------------------------------------------------

  result = esp_now_register_recv_cb(onESPNowReceive);

  if (result != ESP_OK) {
    Serial.print("Failed to register receive callback: ");

    Serial.println(esp_err_to_name(result));

    esp_now_deinit();

    return false;
  }

  // ---------------------------------------------------
  // Send callback
  // ---------------------------------------------------

  result = esp_now_register_send_cb(onESPNowSendActual);

  if (result != ESP_OK) {
    Serial.print("Failed to register send callback: ");

    Serial.println(esp_err_to_name(result));

    esp_now_deinit();

    return false;
  }

  // ---------------------------------------------------
  // Broadcast peer
  // ---------------------------------------------------

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

  pairingModeStarted = millis();

  Serial.println();
  Serial.println("ESP-NOW READY");

  Serial.println("Controller + receiver communication enabled");

  Serial.println();

  return true;
}
sendPairRequestState EspnowController::sendPairRequest(int index) {
  PairPacket pairPacket = {};

  pairPacket.type = CMD_PAIR_REQUEST;

  memcpy(pairPacket.receiverMac, localMac, 6);

  esp_err_t result =
      esp_now_send(receivers[index].mac,
                   reinterpret_cast<uint8_t*>(&pairPacket), sizeof(pairPacket));

  if (result != ESP_OK) {
    Serial.print("PAIR send failed: ");

    Serial.print(result);

    Serial.print(" / ");

    Serial.println(esp_err_to_name(result));

    //
    receivers[index].paired = false;

    return PAIR_ERROR;
  }

  Serial.print("PAIR request sent to: ");

  Serial.println(receivers[index].name);

  return PAIR_SUCCESS;
}

validateSelectedPeerState EspnowController::validateSelectedPeer(int index) {
  if (index < 0 || index >= receiverCount) {
    return INVALID_INDEX;
  }

  if (!receivers[index].used) {
    return INVALID_INDEX;
  }

  // Don't do anything if already selected

  if (index == activeReceiver && receivers[index].paired) {
    return ALREADY_SELECTED;
  }

  // Stop previous receiver

  if (activeReceiver >= 0 && activeReceiver != index) {
    // todo maybe add this back?
    //   stopActiveReceiver();

    // TODO check
    removeController(index);
  }

  // Make sure peer exists

  if (!addPeer(receivers[index].mac, receivers[index].channel)) {
    //

    return PEER_ERROR;
  }

  // Select receiver

  activeReceiver = index;

  receivers[index].paired = false;

  return PASS_CHECK;
}
// todo should this be in app state?
bool EspnowController::hasActiveReceiver() {
  return !(activeReceiver < 0 || activeReceiver >= receiverCount ||
           !receivers[activeReceiver].used);
}

void EspnowController::resetDiscover() {
  // Reset receiver list

  receiverCount = 0;

  activeReceiver = -1;

  for (int i = 0; i < MAX_RECEIVERS; i++) {
    receivers[i] = {};
  }

  // Start discovery

  discoveryActive = true;

  discoveryStarted = millis();

  lastDiscoverySend = 0;

  receiverListNeedsRefresh = true;
}

bool EspnowController::isActiveReceiver(int i) { return i == activeReceiver; }

ReceiverInfo* EspnowController::getReceivers() { return receivers; }
int EspnowController::getActiveReceiver() { return activeReceiver; }
int EspnowController::getNumberOfReceivers() { return receiverCount; }
void EspnowController::setReceiverListNeedsRefresh(bool enable) {
  receiverListNeedsRefresh = enable;
}
bool EspnowController::getReceiverListNeedsRefresh() {
  return receiverListNeedsRefresh;
}