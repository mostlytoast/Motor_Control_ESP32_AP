#include "espnow_controller.h"


// =====================================================
// PEER MANAGEMENT
// =====================================================

// TODO unify names of peer receiver and controller to be peer
bool addPeer(const uint8_t* mac, uint8_t channel) {
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

bool addControllerPeer(const uint8_t* mac) {
  // TODO look into combine controller and peer
  return addPeer(mac, WIFI_CHANNEL);
}

void removePeer(const uint8_t* mac) {
  if (mac == nullptr) {
    return;
  }

  if (esp_now_is_peer_exist(mac)) {
    esp_now_del_peer(mac);
  }
}

// =====================================================
// EVENT QUEUE
// =====================================================

void queueEspNowEvent(EspNowEventType type, const uint8_t* mac,
                      const uint8_t* data, int len) {
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

// =====================================================
// CONTROLLER LIST
// =====================================================

int findReceiverByMac(const uint8_t* mac) {
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

int addOrUpdateReceiver(const DiscoveryPacket* packet) {
  if (packet == nullptr) {
    return -1;
  }

  int existing = findReceiverByMac(packet->receiverMac);

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

int findController(const uint8_t* mac) {
  if (mac == nullptr) {
    return -1;
  }

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (!controllers[i].valid) {
      continue;
    }

    if (macEqual(controllers[i].mac, mac)) {
      return i;
    }
  }

  return -1;
}

bool isPairedController(const uint8_t* mac) { return findController(mac) >= 0; }

int findFreeControllerSlot() {
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

int pairController(const uint8_t* mac, const char* controllerName) {
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

bool removeController(int slot) {
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

bool claimController(const uint8_t* mac) {
  int slot = findController(mac);

  if (slot < 0) {
    return false;
  }

  activeController = slot;

  lastActiveControllerPacket = millis();

  controllers[slot].lastSeen = millis();

  Serial.print("ACTIVE CONTROLLER -> ");

  Serial.println(controllers[slot].name);

  return true;
}

bool isActiveController(const uint8_t* mac) {
  int slot = findController(mac);

  if (slot < 0) {
    return false;
  }

  return slot == activeController;
}

// =====================================================
// BINARY DISCOVERY RESPONSE
// =====================================================

void sendDiscoveryPacket(const uint8_t* destination) {
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
    addControllerPeer(destination);
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

void sendPairResponse(const uint8_t* destination, int slot, bool paired) {
  PairResponse response = {};

  response.type = CMD_PAIR_RESPONSE;

  response.value = paired ? 1 : 0;

  uint8_t receiverMac[6];

  WiFi.macAddress(receiverMac);

  memcpy(response.receiverMac, receiverMac, 6);

  response.controllerSlot = slot >= 0 ? slot : 255;

  response.paired = paired ? 1 : 0;

  strncpy(response.receiverName, RECEIVER_NAME,
          sizeof(response.receiverName) - 1);

  addControllerPeer(destination);

  esp_now_send(destination, reinterpret_cast<uint8_t*>(&response),
               sizeof(response));
}

// =====================================================
// BINARY CONTROLLER STATUS
// =====================================================

void sendControllerStatus(const uint8_t* destination) {
  ESPNowCommand response = {};

  response.type = CMD_CONTROLLER_STATUS;

  response.value = activeController;

  if (activeController >= 0 && activeController < MAX_CONTROLLERS &&
      controllers[activeController].valid) {
    memcpy(response.data, controllers[activeController].mac, 6);
  }

  addControllerPeer(destination);

  esp_now_send(destination, reinterpret_cast<uint8_t*>(&response),
               sizeof(response));
}

// =====================================================
// JSON REASSEMBLY HELPERS
// =====================================================

int findJsonReassembly(const uint8_t* sourceMac) {
  if (sourceMac == nullptr) {
    return -1;
  }

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; i++) {
    if (!jsonReassemblies[i].active) {
      continue;
    }

    if (macEqual(jsonReassemblies[i].sourceMac, sourceMac)) {
      return i;
    }
  }

  return -1;
}

void clearJsonReassembly(int index) {
  if (index < 0 || index >= MAX_JSON_REASSEMBLIES) {
    return;
  }

  memset(&jsonReassemblies[index], 0, sizeof(JsonReassembly));
}

int allocateJsonReassembly(const uint8_t* sourceMac) {
  // ---------------------------------------------------
  // First try to find an existing one
  // ---------------------------------------------------

  int existing = findJsonReassembly(sourceMac);

  if (existing >= 0) {
    return existing;
  }

  // ---------------------------------------------------
  // Find free slot
  // ---------------------------------------------------

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; i++) {
    if (!jsonReassemblies[i].active) {
      clearJsonReassembly(i);

      jsonReassemblies[i].active = true;

      memcpy(jsonReassemblies[i].sourceMac, sourceMac, 6);

      return i;
    }
  }

  // ---------------------------------------------------
  // No free slot.
  //
  // Reuse the oldest one.
  // ---------------------------------------------------

  int oldest = 0;

  for (int i = 1; i < MAX_JSON_REASSEMBLIES; i++) {
    if (jsonReassemblies[i].lastReceived <
        jsonReassemblies[oldest].lastReceived) {
      oldest = i;
    }
  }

  clearJsonReassembly(oldest);

  jsonReassemblies[oldest].active = true;

  memcpy(jsonReassemblies[oldest].sourceMac, sourceMac, 6);

  return oldest;
}

void cleanupJsonReassemblies() {
  const unsigned long now = millis();

  for (int i = 0; i < MAX_JSON_REASSEMBLIES; i++) {
    if (!jsonReassemblies[i].active) {
      continue;
    }

    if (now - jsonReassemblies[i].lastReceived > JSON_REASSEMBLY_TIMEOUT) {
      Serial.print("JSON reassembly timeout from ");

      Serial.println(macToString(jsonReassemblies[i].sourceMac));

      clearJsonReassembly(i);
    }
  }
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

bool sendJsonDocument(const uint8_t* destination, JsonDocument& doc) {
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

  // ---------------------------------------------------
  // Serialize entire JSON document
  // ---------------------------------------------------

  String json;

  serializeJson(doc, json);

  if (json.length() == 0) {
    Serial.println("sendJsonDocument: empty JSON");

    return false;
  }

  // ---------------------------------------------------
  // Calculate number of fragments
  // ---------------------------------------------------

  size_t totalChunks =
      (json.length() + JSON_FRAGMENT_DATA_SIZE - 1) / JSON_FRAGMENT_DATA_SIZE;

  if (totalChunks == 0) {
    return false;
  }

  if (totalChunks > 255) {
    Serial.print("JSON too large: ");

    Serial.print(json.length());

    Serial.println(" bytes. Maximum is 255 chunks.");

    return false;
  }

  // ---------------------------------------------------
  // Message ID
  // ---------------------------------------------------

  uint16_t messageId = ++nextJsonMessageId;

  if (messageId == 0) {
    messageId = ++nextJsonMessageId;
  }

  // ---------------------------------------------------
  // Determine JSON type
  // ---------------------------------------------------

  uint8_t type = 0;

  if (doc["type"].is<uint8_t>()) {
    type = doc["type"].as<uint8_t>();
  }

  // ---------------------------------------------------
  // Send every fragment
  // ---------------------------------------------------

  for (size_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    uint8_t packet[JSON_FRAGMENT_PACKET_SIZE];

    JsonFragmentHeader header = {};

    header.magic = JSON_FRAGMENT_MAGIC;

    header.type = type;

    header.messageId = messageId;

    header.chunkIndex = static_cast<uint8_t>(chunkIndex);

    header.totalChunks = static_cast<uint8_t>(totalChunks);

    memcpy(packet, &header, sizeof(header));

    // -------------------------------------------------
    // Calculate chunk data
    // -------------------------------------------------

    size_t offset = chunkIndex * JSON_FRAGMENT_DATA_SIZE;

    size_t remaining = json.length() - offset;

    size_t chunkLength = min(remaining, JSON_FRAGMENT_DATA_SIZE);

    memcpy(packet + sizeof(header), json.c_str() + offset, chunkLength);

    size_t packetLength = sizeof(header) + chunkLength;

    // -------------------------------------------------
    // Send
    // -------------------------------------------------

    esp_err_t result = esp_now_send(destination, packet, packetLength);

    if (result != ESP_OK) {
      Serial.print("JSON fragment send failed: ");

      Serial.print(result);

      Serial.print(" / ");

      Serial.println(esp_err_to_name(result));

      return false;
    }
  }

  return true;
}
// =====================================================
// JSON COMMAND
// =====================================================

bool sendCommand(uint8_t type, int16_t value) {
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

JsonDocument previousState;
bool previousStateUpdated = false;

bool hasPreviousStateChanged() {
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

bool sendServerState(const uint8_t* destination) {
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
// BROADCAST SERVER STATE
// =====================================================

void broadcastServerState() {
  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (!controllers[i].valid) {
      continue;
    }

    sendServerState(controllers[i].mac);
  }
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

void processStateChanges() {
  const uint8_t* currentControllerMac = controllers[activeController].mac;
  if ((activeReceiver >= 0 && activeReceiver < receiverCount &&
       receivers[activeReceiver].used && receivers[activeReceiver].paired)) {
    bool sent = sendServerState(receivers[activeReceiver].mac);
  } else if (isActiveController(currentControllerMac)) {
    bool sent = sendServerState(currentControllerMac);
  }
}

// =====================================================
// PROCESS JSON SERVER STATE
// =====================================================

void processServerState(const uint8_t* data, int len) {
  if (data == nullptr || len <= 0) {
    return;
  }

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.print("JSON server state parse failed: ");

    Serial.println(error.c_str());

    return;
  }

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

void processJsonCommand(const uint8_t* sourceMac, JsonDocument& doc) {
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
    activeController = controllerSlot;
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

bool processReceiverBinaryPacket(const uint8_t* sourceMac, const uint8_t* data,
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
    if (isPairedController(sourceMac)) {
      claimController(sourceMac);

      sendControllerStatus(sourceMac);

      // Initial state synchronization is intentional.
      sendServerState(sourceMac);
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

// =====================================================
// PROCESS CONTROLLER-SIDE BINARY PACKETS
// =====================================================

bool processControllerBinaryPacket(const uint8_t* sourceMac,
                                   const uint8_t* data, int len) {
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

    if (response.type == CMD_PAIR_RESPONSE) {
      int index = findReceiverByMac(sourceMac);

      if (index >= 0) {
        receivers[index].paired = response.paired != 0;

        receivers[index].lastSeen = millis();

        receiverListNeedsRefresh = true;

        Serial.print("Receiver pairing: ");

        Serial.println(receivers[index].paired ? "SUCCESS" : "FAILED");
      }

      return true;
    }
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
// PROCESS JSON FRAGMENT
//
// Returns true if the packet was recognized as a JSON
// fragment.
//
// When all fragments have arrived, the reconstructed
// JSON is passed to the appropriate JSON handler.
// =====================================================

bool processJsonFragment(const uint8_t* sourceMac, const uint8_t* data,
                         size_t len) {
  if (sourceMac == nullptr || data == nullptr) {
    return false;
  }

  // ---------------------------------------------------
  // Not enough bytes for a header
  // ---------------------------------------------------

  if (len < sizeof(JsonFragmentHeader)) {
    return false;
  }

  JsonFragmentHeader header;

  memcpy(&header, data, sizeof(header));

  // ---------------------------------------------------
  // Not a JSON fragment
  // ---------------------------------------------------

  if (header.magic != JSON_FRAGMENT_MAGIC) {
    return false;
  }

  // ---------------------------------------------------
  // Validate header
  // ---------------------------------------------------

  if (header.totalChunks == 0) {
    Serial.println("Invalid JSON fragment count");

    return true;
  }

  if (header.chunkIndex >= header.totalChunks) {
    Serial.println("Invalid JSON fragment index");

    return true;
  }

  size_t chunkLength = len - sizeof(JsonFragmentHeader);

  if (chunkLength == 0) {
    Serial.println("Empty JSON fragment");

    return true;
  }

  // ---------------------------------------------------
  // Get reassembly slot for this sender
  // ---------------------------------------------------

  int slot = findJsonReassembly(sourceMac);

  // ---------------------------------------------------
  // New message
  // ---------------------------------------------------

  if (slot < 0) {
    slot = allocateJsonReassembly(sourceMac);

    JsonReassembly& assembly = jsonReassemblies[slot];

    assembly.type = header.type;

    assembly.messageId = header.messageId;

    assembly.totalChunks = header.totalChunks;

    assembly.receivedChunks = 0;

    assembly.length = 0;

    memset(assembly.received, 0, sizeof(assembly.received));
  }

  JsonReassembly& assembly = jsonReassemblies[slot];

  // ---------------------------------------------------
  // If this is a different message from the same
  // sender, start over.
  //
  // This also handles a new message arriving before
  // the previous one has completed.
  // ---------------------------------------------------

  if (assembly.messageId != header.messageId ||
      assembly.totalChunks != header.totalChunks ||
      assembly.type != header.type) {


    clearJsonReassembly(slot);

    assembly.active = true;

    memcpy(assembly.sourceMac, sourceMac, 6);

    assembly.type = header.type;

    assembly.messageId = header.messageId;

    assembly.totalChunks = header.totalChunks;

    assembly.receivedChunks = 0;

    assembly.length = 0;

    memset(assembly.received, 0, sizeof(assembly.received));
  }

  assembly.lastReceived = millis();

  // ---------------------------------------------------
  // Ignore duplicate fragment
  // ---------------------------------------------------

  if (assembly.received[header.chunkIndex]) {
    Serial.print("Duplicate JSON chunk ");

    Serial.println(header.chunkIndex);

    return true;
  }

  // ---------------------------------------------------
  // Calculate where this chunk belongs
  // ---------------------------------------------------

  size_t offset =
      static_cast<size_t>(header.chunkIndex) * JSON_FRAGMENT_DATA_SIZE;

  // ---------------------------------------------------
  // Check buffer
  // ---------------------------------------------------

  if (offset + chunkLength >= JSON_REASSEMBLY_BUFFER_SIZE) {
    Serial.println("JSON reassembly buffer overflow");

    clearJsonReassembly(slot);

    return true;
  }

  // ---------------------------------------------------
  // Copy chunk
  // ---------------------------------------------------

  memcpy(assembly.buffer + offset, data + sizeof(JsonFragmentHeader),
         chunkLength);

  assembly.received[header.chunkIndex] = true;

  assembly.receivedChunks++;

  size_t end = offset + chunkLength;

  if (end > assembly.length) {
    assembly.length = end;
  }
  // ---------------------------------------------------
  // Still waiting for chunks
  // ---------------------------------------------------

  if (assembly.receivedChunks < assembly.totalChunks) {
    return true;
  }

  // ---------------------------------------------------
  // COMPLETE
  // ---------------------------------------------------

  assembly.buffer[assembly.length] = '\0';


  // ---------------------------------------------------
  // Parse reconstructed JSON
  // ---------------------------------------------------

  JsonDocument doc;

  DeserializationError error =
      deserializeJson(doc, assembly.buffer, assembly.length);

  if (error) {
    Serial.print("Reassembled JSON parse failed: ");

    Serial.println(error.c_str());

    clearJsonReassembly(slot);

    return true;
  }

  // ---------------------------------------------------
  // Validate type
  // ---------------------------------------------------

  if (!doc["type"].is<uint8_t>()) {
    Serial.println("Reassembled JSON has no type");

    clearJsonReassembly(slot);

    return true;
  }

  uint8_t type = doc["type"].as<uint8_t>();

  // ---------------------------------------------------
  // SERVER STATE
  // ---------------------------------------------------

  if (type == CMD_SERVER_STATE) {
    int receiverIndex = findReceiverByMac(sourceMac);

    if (receiverIndex >= 0) {
      receivers[receiverIndex].lastSeen = millis();
    }

    processServerState(reinterpret_cast<const uint8_t*>(assembly.buffer),
                       assembly.length);
  }

  // ---------------------------------------------------
  // Other JSON commands
  // ---------------------------------------------------

  else {
    processJsonCommand(sourceMac, doc);
  }

  // ---------------------------------------------------
  // Done
  // ---------------------------------------------------

  clearJsonReassembly(slot);

  return true;
}
// =====================================================
// UNIFIED RECEIVE EVENT PROCESSING
// =====================================================

// =====================================================
// UNIFIED RECEIVE EVENT PROCESSING
// =====================================================

void processESPNowEvents() {
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

        int index = findReceiverByMac(event.mac);

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
        int index = findReceiverByMac(event.mac);

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

bool pairWithReceiver(int index) {
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

void sendDiscovery() {
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

void processDiscovery() {
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

void ESPNow_checkFailsafe() {
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

void ESPNow_updatePairingMode() {
  if (pairingMode && pairingModeStarted != 0 &&
      millis() - pairingModeStarted > DEFAULT_PAIRING_WINDOW_MS) {
    pairingMode = false;

    Serial.println("Pairing window expired");
  }
}

// =====================================================
// HEARTBEAT
// =====================================================

void sendHeartbeat() {
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

void espnowUpdate() {
  // ---------------------------------------------------
  // Remove incomplete JSON messages that timed out
  // ---------------------------------------------------

  cleanupJsonReassemblies();

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

void onESPNowReceive(const esp_now_recv_info_t* info, const uint8_t* data,
                     int len) {
  if (info == nullptr || data == nullptr || len <= 0) {
    return;
  }

  queueEspNowEvent(ESP_NOW_EVENT_RECEIVE, info->src_addr, data, len);
}

// =====================================================
// SEND CALLBACK
// =====================================================

void onESPNowSendActual(const wifi_tx_info_t* tx_info,
                        esp_now_send_status_t status) {
  if (tx_info == nullptr || espNowEventQueue == nullptr) {
    return;
  }

  EspNowEvent event = {};

  event.type = ESP_NOW_EVENT_SEND;

  memcpy(event.mac, tx_info->des_addr, 6);

  event.sendStatus = status;

  xQueueSend(espNowEventQueue, &event, 0);
}

// =====================================================
// SETUP
// =====================================================

bool setupESPNow(bool wifi) {
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
sendPairRequestState sendPairRequest(int index) {
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

validateSelectedPeerState validateSelectedPeer(int index) {
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
bool hasActiveReceiver() {
  return !(activeReceiver < 0 || activeReceiver >= receiverCount ||
           !receivers[activeReceiver].used);
}

void resetDiscover() {
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

bool isActiveReceiver(int i) { return i == activeReceiver; }