#ifndef SHARED_TRACK_TAG_HPP
#define SHARED_TRACK_TAG_HPP

#include "../utils/uidLogic.h"

class Tag {
 public:
  Tag(Uid uid, Uid connectedTagUid) {
    this->uid = uid;
    this->connectedTagUid = connectedTagUid;
  }

  Uid getUid() const { return uid; }

  Uid getConnectedTagUid() const { return connectedTagUid; }

  void setUid(Uid uid) { this->uid = uid; }

  void setConnectedTagUid(Uid connectedTagUid) {
    this->connectedTagUid = connectedTagUid;
  }

  bool operator==(const Tag& other) const {
    if (!uid.equals(other.getUid())) return false;
    if (!connectedTagUid.equals(other.getConnectedTagUid())) return false;

    return true;
  }

  bool operator!=(const Tag& other) const { return !(*this == other); }

 private:
  Uid uid;
  Uid connectedTagUid;

  //   TODO think of moving point from tracks to tag
};

#endif  // SHARED_TRACK_TAG_HPP
