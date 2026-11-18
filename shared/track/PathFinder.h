#ifndef TRACK_PATH_FINDER_H
#define TRACK_PATH_FINDER_H

#include <cstddef>
#include <vector>

#include "Tracks.hpp"
#include "../utils/Uids.hpp"


class TrackPathFinder {
 public:
  explicit TrackPathFinder(const Tracks& tracks_);

  /**
   * Find the shortest path between two tags using BFS.
   *
   * @param startUid Starting tag UID.
   * @param destinationUid Destination tag UID.
   * @param path Output vector containing tag UIDs in traversal order.
   * @return true if a path was found, false otherwise.
   */
  bool findPath(const Uid& startUid, const Uid& destinationUid,
                Uids& path) const;

 private:
  static constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);

  const Tracks& tracks;

  /**
   * Find the index of a tag in the node collection.
   */
  size_t findNode(const std::vector<const Tag*>& nodes, const Uid& uid) const;

  /**
   * Reconstruct the path using the BFS parent array.
   */
  bool reconstructPath(const std::vector<const Tag*>& nodes,
                       const std::vector<size_t>& parent, size_t start,
                       size_t destination, Uids& path) const;
};

#endif  // TRACK_PATH_FINDER_H