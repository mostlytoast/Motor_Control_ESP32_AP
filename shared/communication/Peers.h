#ifndef PEERS_H
#define PEERS_H
#include <Arduino.h>

#include <iostream>
#include <string>
#include <vector>

#include "../utils/MacUtils.h"
#include "Peer.h"
// TODO make it dynamic sized list
class Peers {
 public:
  friend std::ostream& operator<<(std::ostream& os, const Peers& p);

  int getNumberOfPeers();

  // Peer* findPeer(int index_);

  Peer* findPeer(const uint8_t* mac_);

  Peer* addPeer(Peer* peer_);

  void removePeer(const uint8_t* mac_);

  void removePeer(Peer* peer_);

  void removePeers();

  std::vector<Peer> getActivePeersWithRole(PeerRole role);

  std::vector<Peer> getActiveControllerPeers();

  std::vector<Peer> getActiveReceiverPeers();

  std::vector<Peer> getActivePeers();
  std::vector<Peer> getPeers();

  Peer* getActiveReceiverPeer();

  bool isPeerAdded(Peer* peer_);

 private:
  std::vector<Peer> peersList;
};
extern Peers peers;
#endif  // PEERS_H