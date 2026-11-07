#include "Peers.h"
Peers peers;

std::ostream& operator<<(std::ostream& os, const Peers& p) {
  for (const Peer& peer : p.peersList) {
    os << peer;
  }
  return os;
}

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

  std::cout << "Adding peer: " << *peer_ << "\n";

  peersList.emplace_back(*peer_);

  // peersList.emplace_back(mac_, channel_, name_);
  return &peersList.back();
}

void Peers::removePeer(const uint8_t* mac_) {
  auto peer =
      std::find_if(peersList.begin(), peersList.end(),
                   [&mac_](Peer& p) { return macEqual(p.getMac(), mac_); });

  if (peer != peersList.end()) {
    std::cout << "removing: " << *peer << "\n";
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
std::vector<Peer> Peers::getActivePeers() {
  // First get all peers regardless of role, then filter by state
  std::vector<Peer> activePeers;

  for (const Peer& peer : peersList) {
    if (peer.getState() == PeerState::ACTIVE) {
      activePeers.push_back(peer);
    }
  }

  return activePeers;
}

std::vector<Peer> Peers::getActiveReceiverPeers() {
  std::vector<Peer> activePeers;

  for (const Peer& peer : peersList) {
    if (peer.getState() == PeerState::ACTIVE &&
        peer.getRole() == PeerRole::RECEIVER) {
      activePeers.push_back(peer);
    }
  }

  return activePeers;
}

std::vector<Peer> Peers::getActiveControllerPeers() {
  std::vector<Peer> activePeers;

  for (const Peer& peer : peersList) {
    if (peer.getState() == PeerState::ACTIVE &&
        peer.getRole() == PeerRole::CONTROLLER) {
      activePeers.push_back(peer);
    }
  }

  return activePeers;
}

std::vector<Peer> Peers::getPeers() { return peersList; }

Peer* Peers::getActiveReceiverPeer() {
  auto it = std::find_if(peersList.begin(), peersList.end(), [](Peer& p) {
    return (p.getState() == PeerState::ACTIVE) && p.getIsActivePeerReceiver();
  });

  return it != peersList.end() ? &(*it) : nullptr;
}

bool Peers::isPeerAdded(Peer* peer_) {
  if (findPeer(peer_->getMac()) != nullptr) {
    return true;
  }
  return false;
}
