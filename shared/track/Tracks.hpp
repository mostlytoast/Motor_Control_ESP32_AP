
#ifndef TRACKS_HPP
#define TRACKS_HPP

#include <algorithm>
#include <vector>

#include "../SharedConfig.h"
#include "Track.hpp"

class Tracks {
 protected:
  static constexpr uint8_t MAX_TRACKS = 100;

 public:
  Tracks() { tracks.reserve(MAX_TRACKS); }
  Track* findTrackByUid(Uid uid) {
    if (!uid.isValid()) return nullptr;

    for (Track& track : tracks) {
      if (track.getTags().findTagByUid(uid) != nullptr) {
        return &track;
      }
    }

    return nullptr;
  }
  bool isFull(){
    return tracks.size() >= MAX_TRACKS;
  }
  Tag* findTagByUid(Uid uid) {
    if (!uid.isValid()) return nullptr;

    for (Track& track : tracks) {
      for (Tag& tag : track.getTags()) {
        if (tag.getUid().isValid() && tag.getUid().equals(uid)) {
          return &tag;
        }
      }
    }

    return nullptr;
  }

  void addTrack(const Track& track) { tracks.emplace_back(track); }

  bool removeTrack(const Track* track) {
    if (!track) return false;

    auto it = std::find_if(tracks.begin(), tracks.end(),
                           [track](const Track& p) { return p == *track; });

    if (it != tracks.end()) {
      tracks.erase(it);
      return true;
    }

    return false;
  }

  bool doUidsBelongToSameTrack(const Uid& uidA, const Uid& uidB) {
    Track* track = findTrackByUid(uidA);
    if (!track) return false;

    for (const Tag& tag : track->getTags()) {
      if (tag.getUid().equals(uidB)) {
        return true;
      }
    }

    return false;
  }

  bool doesTrackExist(Uid uid) { return findTrackByUid(uid) != nullptr; }

  size_t size() const { return tracks.size(); }

  bool empty() const { return tracks.empty(); }

  void clear() { tracks.clear(); }

  Track& operator[](size_t idx) { return tracks[idx]; }

  const Track& operator[](size_t idx) const { return tracks[idx]; }

  auto begin() { return tracks.begin(); }
  auto end() { return tracks.end(); }

  auto begin() const { return tracks.begin(); }
  auto end() const { return tracks.end(); }

  void emplace_back(const Track& track) { tracks.emplace_back(track); }
  Track& back() { return tracks.back(); }

  const Track& back() const { return tracks.back(); }

  bool operator==(const Tracks& other) const {
    if (tracks.size() != other.tracks.size()) return false;
    for (size_t i = 0; i < tracks.size(); ++i) {
      if (tracks[i] != other.tracks[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const Tracks& other) const { return !(*this == other); }

 private:
  std::vector<Track> tracks;
};

extern Tracks tracks;

#endif
