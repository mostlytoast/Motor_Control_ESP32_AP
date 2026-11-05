#ifndef TRACK_PATH_FINDER_H
#define TRACK_PATH_FINDER_H

#include <vector>
#include <cstddef>

#include "map.h"

class TrackPathFinder {
 public:
  explicit TrackPathFinder(const TrackMap& map);

  /**
   * Find the shortest path between two tags using BFS.
   *
   * @param startUid Starting tag UID.
   * @param destinationUid Destination tag UID.
   * @param path Output vector containing tag UIDs in traversal order.
   * @return true if a path was found, false otherwise.
   */
  bool findPath(const Uid& startUid,
                const Uid& destinationUid,
                std::vector<Uid>& path) const;

 private:
  static constexpr size_t INVALID_INDEX =
      static_cast<size_t>(-1);

  const TrackMap& map_;

  /**
   * Find the index of a tag in the node collection.
   */
  size_t findNode(
      const std::vector<const Tag*>& nodes,
      const Uid& uid) const;

  /**
   * Reconstruct the path using the BFS parent array.
   */
  bool reconstructPath(
      const std::vector<const Tag*>& nodes,
      const std::vector<size_t>& parent,
      size_t start,
      size_t destination,
      std::vector<Uid>& path) const;
};

#endif  // TRACK_PATH_FINDER_H