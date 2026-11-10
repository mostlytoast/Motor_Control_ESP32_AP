#ifndef PEER_H
#define PEER_H
#include <Arduino.h>

#include <iostream>
#include <string>
#include <vector>

#include "../utils/MacUtils.h"
enum class PeerRole : uint8_t { UNKNOWN, CONTROLLER, RECEIVER };

enum class PeerState : uint8_t {
  DISCOVERED,
  PAIRING,
  PAIRED,
  ACTIVE,
  TIMED_OUT,
  NONE
};

class Peer {
 public:
  friend std::ostream& operator<<(std::ostream& os, const Peer& p);

  // Peer.h
  Peer(const uint8_t* mac_, uint8_t channel_, const char* name_);

  const unsigned long getLastSeen() const;
  void setLastSeen(unsigned long time);

  const uint8_t* getMac() const;
  void setMac(const uint8_t* mac_);

  uint8_t getChannel();
  void setChannel(uint8_t channel_);

  const char* getName() const;
  void setName(const char* name_);

  void setRole(PeerRole role_);
  const PeerRole getRole() const;

  void setState(PeerState state_);
  PeerState getState() const;

  bool getIsActivePeer();

  bool getIsActivePeerReceiver();
  bool getIsActivePeerController();

  String toString();

 private:
  uint8_t mac[MAC_LENGTH] = {0};
  char name[33];
  uint8_t channel;

  PeerRole role = PeerRole::UNKNOWN;

  PeerState state = PeerState::NONE;
  // can these be condensed into one state?

  unsigned long lastSeen = 0;
};
extern Peer peer;
#endif  // PEERS_H