#include "PathFinder.h"

 TrackPathFinder::TrackPathFinder(const TrackMap& map) : map_(map) {}

// Find the shortest path between two tags.
// Returns true if a path exists.
bool TrackPathFinder::findPath(const Uid& startUid, const Uid& destinationUid,
                               std::vector<Uid>& path) const {
  path.clear();

  const auto& tracks = map_.getTracks();

  // --------------------------------------------------
  // Collect every tag in the map.
  // --------------------------------------------------

  std::vector<const Tag*> nodes;

  for (const Track& track : tracks) {
    for (const Tag& tag : track.getTags()) {
      nodes.emplace_back(&tag);
    }
  }

  // --------------------------------------------------
  // Find start and destination nodes.
  // --------------------------------------------------

  const size_t start = findNode(nodes, startUid);
  const size_t destination = findNode(nodes, destinationUid);

  if (start == INVALID_INDEX || destination == INVALID_INDEX) {
    return false;
  }

  if (start == destination) {
    path.emplace_back(startUid);
    return true;
  }

  // --------------------------------------------------
  // BFS storage.
  // --------------------------------------------------

  const size_t count = nodes.size();

  std::vector<size_t> queue;
  std::vector<size_t> parent(count, INVALID_INDEX);
  std::vector<bool> visited(count, false);

  queue.reserve(count);

  queue.emplace_back(start);
  visited[start] = true;

  size_t head = 0;

  // --------------------------------------------------
  // Breadth First Search.
  // --------------------------------------------------

  while (head < queue.size()) {
    const size_t current = queue[head++];

    const Tag& tag = *nodes[current];

    // ----------------------------------------------
    // Check internal connections.
    // ----------------------------------------------

    for (const Uid& connectedUid : tag.getInternalConnectedTagUids()) {
      size_t next = findNode(nodes, connectedUid);

      if (next == INVALID_INDEX || visited[next]) {
        continue;
      }

      visited[next] = true;
      parent[next] = current;

      if (next == destination) {
        return reconstructPath(nodes, parent, start, destination, path);
      }

      queue.emplace_back(next);
    }

    // ----------------------------------------------
    // Check external connection.
    // ----------------------------------------------

    const Uid connectedUid = tag.getConnectedTagUid();

    if (connectedUid.isValid()) {
      size_t next = findNode(nodes, connectedUid);

      if (next == INVALID_INDEX || visited[next]) {
        continue;
      }

      visited[next] = true;
      parent[next] = current;

      if (next == destination) {
        return reconstructPath(nodes, parent, start, destination, path);
      }

      queue.emplace_back(next);
    }
  }

  // No path exists.
  return false;
}

// --------------------------------------------------
// FIND NODE
// --------------------------------------------------

size_t TrackPathFinder::findNode(const std::vector<const Tag*>& nodes,
                                 const Uid& uid) const {
                                    // todo make this use track functions directly? 
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i]->getUid().equals(uid)) {
      return i;
    }
  }

  return INVALID_INDEX;
}

// --------------------------------------------------
// RECONSTRUCT PATH
// --------------------------------------------------

bool TrackPathFinder::reconstructPath(const std::vector<const Tag*>& nodes,
                                      const std::vector<size_t>& parent,
                                      size_t start, size_t destination,
                                      std::vector<Uid>& path) const {
  std::vector<Uid> reversePath;

  size_t current = destination;

  while (current != start) {
    reversePath.emplace_back(nodes[current]->getUid());

    current = parent[current];

    if (current == INVALID_INDEX) {
      return false;
    }
  }

  reversePath.emplace_back(nodes[start]->getUid());

  // Reverse into start -> destination order.
  for (auto it = reversePath.rbegin(); it != reversePath.rend(); ++it) {
    path.emplace_back(*it);
  }

  return true;
}
