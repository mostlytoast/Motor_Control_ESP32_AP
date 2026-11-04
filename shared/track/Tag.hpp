#ifndef SHARED_TRACK_TAG_HPP
#define SHARED_TRACK_TAG_HPP

#include <string.h>

#include "../utils/uidLogic.h"
class Tag {
 public:
  Tag(const char* tagString) { fromString(tagString); }
  Tag(Uid uid_, Uid connectedTagUid_, Point point_) {
    uid = uid_;
    connectedTagUid = connectedTagUid_;
    point = point_;
  }

  Uid getUid() const { return uid; }

  void setUid(Uid uid_) { uid = uid_; }

  Uid getConnectedTagUid() const { return connectedTagUid; }

  void setConnectedTagUid(Uid connectedTagUid_) {
    connectedTagUid = connectedTagUid_;
  }

  // TODO make sure no memory issues
  Point& getPoint() { return point; }
  Point getPoint() const { return point; }
  // void setPoint(Point point_) { point = point_; }
  void setPoint(Point point_) {
    point.setX(point_.getX());
    point.setY(point_.getY());
    point.setHeading(point_.getHeading());
  }

  bool operator==(const Tag& other) const {
    if (!uid.equals(other.getUid())) return false;
    if (!connectedTagUid.equals(other.getConnectedTagUid())) return false;
    if (point != other.getPoint()) return false;

    return true;
  }
  String toString() const {
    // TODO convert to json function
    String result = uid.toString();
    result += "|";
    result += connectedTagUid.toString();
    result += "|";
    result += point.toString();
    return result;
  }

  // Convert string to Point: x:y:heading
  bool fromString(const char* tagString) {
    // TODO convert to json function
    if (tagString == nullptr) return false;

    // Find the separators.
    const char* first = strchr(tagString, '|');
    if (!first) return false;

    const char* second = strchr(first + 1, '|');
    if (!second) return false;

    // Extract individual values.
    String newUidString = String(tagString).substring(0, first - tagString);

    String newConnectedTagUidString =
        String(first + 1).substring(0, second - first - 1);

    String newPointString = String(second + 1);

    // Construct temporary objects.
    Uid newUid(newUidString.c_str());
    Uid newConnectedTagUid(newConnectedTagUidString.c_str());
    Point newPoint(newPointString.c_str());

    // Update only after parsing.
    uid = newUid;
    connectedTagUid = newConnectedTagUid;
    point = newPoint;

    return true;
  }
  bool operator!=(const Tag& other) const { return !(*this == other); }

 private:
  Uid uid;
  Uid connectedTagUid;
  Point point;
  //   TODO think of moving point from tracks to tag
};

#endif  // SHARED_TRACK_TAG_HPP
