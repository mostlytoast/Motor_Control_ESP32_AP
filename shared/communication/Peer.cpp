
#include "Peer.h"

#include <cstring>
Peer::Peer(const uint8_t* mac_, uint8_t channel_, const char* name_) {
  setMac(mac_);
  setChannel(channel_);
  setName(name_);
}

const unsigned long Peer::getLastSeen() const { return lastSeen; }

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

void Peer::setState(PeerState state_) { state = state_; }
PeerState Peer::getState() const { return state; }

bool Peer::getIsActivePeer() { return role != PeerRole::UNKNOWN; }

bool Peer::getIsActivePeerReceiver() { return role == PeerRole::RECEIVER; }

bool Peer::getIsActivePeerController() { return role == PeerRole::CONTROLLER; }

const char* peerRoleToString(PeerRole role_) {
  switch (role_) {
    case PeerRole::UNKNOWN:
      return "Unkown";
    case PeerRole::RECEIVER:
      return "Receiver";
    case PeerRole::CONTROLLER:
      return "Controller";
    default:
      return "Unknown";
  }
}
const char* peerStateToString(PeerState state_) {
  switch (state_) {
    case PeerState::DISCOVERED:
      return "Discovered";
    case PeerState::PAIRING:
      return "Pairing";
    case PeerState::PAIRED:
      return "Paired";
    case PeerState::ACTIVE:
      return "Active";
    case PeerState::TIMED_OUT:
      return "Timed Out";
    case PeerState::NONE:
      return "None";
    default:
      return "Unknown";
  }
}

std::ostream& operator<<(std::ostream& os, const Peer& p) {
  os << "Peer: mac: " << macToString(p.getMac()).c_str() << ", name: " << p.name
     << ", last seen:" << p.lastSeen << ", channel: " << (int)p.channel
     << ", role: " << peerRoleToString(p.role)
     << ", state: " << peerStateToString(p.state) << "\n";

  return os;  // Return the stream reference to allow chaining (e.g., std::cout
              // << a << b;)
}

// template <>
// struct std::formatter<Peer> : std::formatter<std::string> {
//     auto format(const Peer& p, std::format_context& ctx) const {
//         return std::format_to(ctx.out(), "({}, {})", p.name, p.lastSeen);
//   // os << "Peer: mac: " << macToString(p.getMac()).c_str() << ", channel: "
//   << p.channel << ", role: " << (p.role == PeerRole::UNKNOWN ? "Unkown" :
//   (p.role == PeerRole::RECEIVER  ? "Receiver" : "Controller")) << ", name: "
//   << p.name << ", last seen:" << p.lastSeen << ", valid: " << p.valid << ",
//   used: " << p.used << ", paired: " << p.paired;

//     }
// };