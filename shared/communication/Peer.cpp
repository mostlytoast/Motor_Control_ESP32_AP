
#include "Peer.h"

#include <cstring>
Peer::Peer(const uint8_t* mac_, uint8_t channel_, const char* name_) {
  setMac(mac_);
  setChannel(channel_);
  setName(name_);
}

const bool Peer::getIsUsed() const { return used; }

void Peer::setIsUsed(bool enable) { used = enable; }

const bool Peer::getIsPaired() const { return paired; }

void Peer::setIsPaired(bool enable) { paired = enable; }

const bool Peer::getIsValid() const { return valid; }

void Peer::setIsValid(bool enable) { valid = enable; }

unsigned long Peer::getLastSeen() { return lastSeen; }

void Peer::setLastSeen(unsigned long time) { lastSeen = time; }

const uint8_t* Peer::getMac() const { return mac; }

void Peer::setMac(const uint8_t* mac_) { memcpy(mac, mac_, MAC_LENGTH); }

uint8_t Peer::getChannel() { return channel; }

void Peer::setChannel(uint8_t channel_) { channel = channel_; }

const char* Peer::getName() const { return name; }

void Peer::setName(const char* name_) {
  if (name_ == nullptr) {
    name[0] = '\0';
    return;
  }

  strncpy(name, name_, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
}

void Peer::setRole(PeerRole role_) { role = role_; }
const PeerRole Peer::getRole() const { return role; }

bool Peer::getIsActivePeer() { return role != PeerRole::UNKNOWN; }

bool Peer::getIsActivePeerReceiver() { return role == PeerRole::RECEIVER; }

bool Peer::getIsActivePeerController() { return role == PeerRole::CONTROLLER; }
