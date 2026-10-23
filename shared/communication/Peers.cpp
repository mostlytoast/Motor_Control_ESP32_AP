#include "Peers.h"
Peers peers;
int Peers::getNumberOfPeers() { return static_cast<int>(peersList.size()); }

// Peer* Peers::findPeer(int index_) { return &peersList.at(index_); }

Peer* Peers::findPeer(const uint8_t* mac_) {
  if (mac_ == nullptr) {
    return nullptr;
  }

  auto it = std::find_if(peersList.begin(), peersList.end(), [mac_](Peer& p) {
    return macEqual(p.getMac(), mac_);
  });

  return it != peersList.end() ? &(*it) : nullptr;
}

Peer* Peers::addPeer(Peer* peer_) {
  if (peer_ == nullptr) {
    Serial.println("Failed to add peer: nullptr");
    return nullptr;
  }

  if (isPeerAdded(peer_)) {
    Serial.println("Peer already exists");
    return nullptr;
  }
  Serial.print("Adding receiver to list ");
  Serial.print(peer_->getName());
  Serial.print(peer_->getRole() == PeerRole::RECEIVER
                   ? "receiver"
                   : (peer_->getRole() == PeerRole::CONTROLLER ? "controller"
                                                               : "unkown"));

  char macText[20];
  formatMac(peer_->getMac(), macText, sizeof(macText));
  Serial.println(macText);

  peersList.emplace_back(*peer_);
  // peersList.emplace_back(mac_, channel_, name_);
  return &peersList.back();
}

void Peers::removePeer(const uint8_t* mac_) {
  auto peer =
      std::find_if(peersList.begin(), peersList.end(),
                   [&mac_](Peer& p) { return macEqual(p.getMac(), mac_); });
  if (peer != peersList.end()) {
    peersList.erase(peer);
  }
}

void Peers::removePeer(Peer* peer_) { removePeer(peer_->getMac()); }

void Peers::removePeers() { peersList.clear(); }

std::vector<Peer> Peers::getActivePeersWithRole(PeerRole role) {
  std::vector<Peer> activePeers;

  for (const Peer& peer : peersList) {
    if (peer.getRole() == role) {
      activePeers.push_back(peer);
    }
  }

  return activePeers;
}

std::vector<Peer> Peers::getActiveControllerPeers() {
  return getActivePeersWithRole(PeerRole::CONTROLLER);
}

std::vector<Peer> Peers::getActiveReceiverPeers() {
  return getActivePeersWithRole(PeerRole::RECEIVER);
}

std::vector<Peer> Peers::getActivePeers() {
  std::vector<Peer> activeReceiverPeers = getActiveReceiverPeers();
  std::vector<Peer> activeControllerPeers = getActiveControllerPeers();

  activeReceiverPeers.insert(activeReceiverPeers.end(),
                             activeControllerPeers.begin(),
                             activeControllerPeers.end());

  return activeReceiverPeers;
}

Peer* Peers::getActiveReceiverPeer() {
  auto it = std::find_if(peersList.begin(), peersList.end(), [](Peer& p) {
    return p.getIsUsed() && p.getIsActivePeerReceiver();
  });

  return it != peersList.end() ? &(*it) : nullptr;
}

bool Peers::isPeerAdded(Peer* peer_) {
  if (findPeer(peer_->getMac()) != nullptr) {
    return true;
  }
  return false;
}
