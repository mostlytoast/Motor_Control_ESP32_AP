#pragma once
#include <Arduino.h>

#include "Uid.hpp"
// #include <Arduino.h> cant include as this script gets imported by map which
// is used in webassm
#include <stdint.h>
#include <string.h>

#include <vector>
class Uids {
 public:
  Uids() { uids.reserve(MAX_UIDS); }

  Uid* findUid(Uid uid) {
    if (!uid.isValid()) return nullptr;

    for (Uid& uid_ : uids) {
      if (uid_.isValid() && uid_.equals(uid)) {
        return &uid_;
      };
    }

    return nullptr;
  }

  void addUid(const Uid& uid) { uids.emplace_back(uid); }

  bool removeUid(const Uid* uid) {
    if (!uid) return false;

    auto it = std::find_if(uids.begin(), uids.end(),
                           [uid](const Uid& p) { return p == *uid; });

    if (it != uids.end()) {
      uids.erase(it);
      return true;
    }

    return false;
  }

  bool doesUidExist(Uid uid) { return findUid(uid) != nullptr; }

  size_t size() const { return uids.size(); }

  bool empty() const { return uids.empty(); }

  void clear() { uids.clear(); }

  Uid& operator[](size_t idx) { return uids[idx]; }

  const Uid& operator[](size_t idx) const { return uids[idx]; }

  auto begin() { return uids.begin(); }
  auto end() { return uids.end(); }

  auto begin() const { return uids.begin(); }
  auto end() const { return uids.end(); }

  auto rbegin() { return uids.rbegin(); }
  auto rend() { return uids.rend(); }

  auto rbegin() const { return uids.rbegin(); }
  auto rend() const { return uids.rend(); }

  void emplace_back(const Uid& uid) { uids.emplace_back(uid); }
  Uid& back() { return uids.back(); }

  const Uid& back() const { return uids.back(); }

  bool operator==(const Uids& other) const {
    if (uids.size() != other.uids.size()) return false;
    for (size_t i = 0; i < uids.size(); ++i) {
      if (uids[i] != other.uids[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const Uids& other) const { return !(*this == other); }

 private:
  static constexpr uint8_t MAX_UIDS = 100;

  std::vector<Uid> uids;
};