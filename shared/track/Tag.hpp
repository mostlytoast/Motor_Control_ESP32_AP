#ifndef SHARED_TRACK_TAG_HPP
#define SHARED_TRACK_TAG_HPP

#include <ArduinoJson.h>
#include <string.h>

#include "../utils/uidLogic.h"
class Tag {
 public:
  Tag() {}

  // Tag(const char* tagString) { fromString(tagString); }
  Tag(bool& success, JsonObject obj, bool condensedJson = false) {
    if (condensedJson) {
      success = fromCondensedJson(obj);
    } else {
      success = fromJson(obj);
    }
  }

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
    Serial.println("uid add contag");
    if (!uid.isValid()) {
      Serial.println("uid not valid");
    } else {
      for (const Uid& existing : internalConnectedTagUids) {
        if (existing.equals(uid)) {
          return;
        }
      }

      internalConnectedTagUids.emplace_back(uid);
    }
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

  bool fromJson(JsonObject& obj) {
    // Reset existing state before loading.
    internalConnectedTagUids.clear();

    const char* uidString = obj["uid"] | "";
    const char* connectedTagUidString = obj["connectedTagUid"] | "";
    const char* pointString = obj["point"] | "";
    if (strcmp(uidString, "") | strcmp(connectedTagUidString, "") |
        strcmp(pointString, "")) {
      return false;
    }
    uid.fromString(uidString);
    connectedTagUid.fromString(connectedTagUidString);
    point.fromString(pointString);

    if (!uid.isValid()) {
      return false;
    }
    JsonArrayConst array = obj["internalConnectedTagUids"].as<JsonArrayConst>();

    for (JsonVariantConst value : array) {
      const char* internalTagUidString = value.as<const char*>();

      if (internalTagUidString != nullptr && strlen(internalTagUidString) > 0) {
        internalConnectedTagUids.emplace_back(internalTagUidString);
      }
    }

    return true;
  }

  void toJson(JsonObject& obj) {
    if (obj.isNull()) {
      return;
    }

    obj["uid"] = uid.toString();
    obj["connectedTagUid"] = connectedTagUid.toString();
    obj["point"] = point.toString();

    JsonArray connectionArray = obj["internalConnectedTagUids"].to<JsonArray>();

    for (const Uid& internalUid : internalConnectedTagUids) {
      connectionArray.add(internalUid.toString());
    }
  }
  bool fromCondensedJson(JsonObject& obj) {
    internalConnectedTagUids.clear();

    const char* uidString = obj["uid"] | "";
    if (strcmp(uidString, "")) {
      return false;
    }
    uid.fromString(uidString);
    if (!uid.isValid()) {
      return false;
    }
    JsonArrayConst array = obj["internalConnectedTagUids"].as<JsonArrayConst>();

    for (JsonVariantConst value : array) {
      const char* internalTagUidString = value.as<const char*>();

      if (internalTagUidString != nullptr && strlen(internalTagUidString) > 0) {
        internalConnectedTagUids.emplace_back(internalTagUidString);
      }
    }

    return true;
  }

  void toCondensedJson(JsonObject& obj) {
    if (obj.isNull()) {
      return;
    }

    obj["uid"] = uid.toString();

    JsonArray connectionArray = obj["internalConnectedTagUids"].to<JsonArray>();

    for (const Uid& internalUid : internalConnectedTagUids) {
      connectionArray.add(internalUid.toString());
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
  // bool fromString(const char* tagString) {
  //   if (tagString == nullptr) {
  //     return false;
  //   }

  //   JsonDocument doc;

  //   DeserializationError error = deserializeJson(doc, tagString);

  //   if (error) {
  //     return false;
  //   }

  //   bool success = false;
  //   fromJson(success, doc.to<JsonObject>());

  //   return success;
  // }
  bool operator!=(const Tag& other) const { return !(*this == other); }

 private:
  Uid uid;
  Uid connectedTagUid;
  std::vector<Uid> internalConnectedTagUids;
  Point point;
  //   TODO think of moving point from tracks to tag
};

#endif  // SHARED_TRACK_TAG_HPP
