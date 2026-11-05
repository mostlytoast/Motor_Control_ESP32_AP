#ifndef SHARED_TRACK_TAG_HPP
#define SHARED_TRACK_TAG_HPP

#include <ArduinoJson.h>
#include <string.h>

#include "../utils/uidLogic.h"
class Tag {
 public:
  Tag(const char* tagString) { fromString(tagString); }
  Tag(bool& success, JsonDocument doc) { fromJson(success, doc); }

  Tag(Uid uid_, Uid connectedTagUid_, Point point_,
      std::vector<Uid> internalConnectedTagUids_ = {}) {
    uid = uid_;
    connectedTagUid = connectedTagUid_;
    internalConnectedTagUids = internalConnectedTagUids_;
    point = point_;
  }

  Uid getUid() const { return uid; }

  void setUid(Uid uid_) { uid = uid_; }

  Uid getConnectedTagUid() const { return connectedTagUid; }

  const std::vector<Uid>& getInternalConnectedTagUids() const {
    return internalConnectedTagUids;
  }

  void addInternalConnectedTagUid(Uid uid) {
    internalConnectedTagUids.emplace_back(uid);
  }

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

  void fromJson(bool& success, JsonDocument& doc) {
    success = false;

    const char* uidString = doc["uid"].as<const char*>();

    const char* connectedTagUid = doc["connectedTagUid"].as<const char*>();

    const char* point = doc["point"].as<const char*>();

    JsonArray array = doc["internalConnectedTagUids"].as<JsonArray>();
    if (uidString == nullptr) {
      uidString = "";
    }
    Uid uid;
    uid.fromString(uidString);
    Uid connectedUid;
    connectedUid.fromString(connectedTagUid);
    Serial.print("pointasdfasasdfsadf");
    Serial.println(Point(point).toString());
    for (const char* internalTagUidString : array) {
      internalConnectedTagUids.emplace_back(internalTagUidString);
    }
    success = true;
  }
  void toJson(JsonObject& obj) {
    if (obj.isNull()) {
      return;
    }

    obj["uid"] = uid.toString();
    obj["connectedTagUid"] = connectedTagUid.toString();
    obj["point"] = point.toString();
    JsonArray connectionArray = obj["internalConnectedTagUids"].to<JsonArray>();

    for (int i = 0; i < internalConnectedTagUids.size(); i++) {
      JsonObject array = connectionArray.add<JsonObject>();

      array[("tag" + std::to_string(i)).c_str()] =
          internalConnectedTagUids[i].toString();
    }
  }

  String toString() {
    String result;
    JsonObject obj;
    toJson(obj);
    serializeJson(obj, result);
    return result;
  }

  // Convert string to Point: x:y:heading
  bool fromString(const char* tagString) {
    // TODO convert to json function

    if (tagString == nullptr) return false;

    JsonDocument doc;

    if (deserializeJson(doc, tagString, sizeof(tagString))) {
      return false;
    }

    bool success;
    fromJson(success, doc);

    return success;
  }
  bool operator!=(const Tag& other) const { return !(*this == other); }

 private:
  Uid uid;
  Uid connectedTagUid;
  std::vector<Uid> internalConnectedTagUids;
  Point point;
  //   TODO think of moving point from tracks to tag
};

#endif  // SHARED_TRACK_TAG_HPP
