
#ifndef STATION_HPP
#define STATION_HPP
#include "Point.hpp"
#include "Tag.hpp"

class Station {
 public:
 public:  // Constructor
  Station() = default;
  Station(bool& success, JsonObject doc, bool condensedJson = false) {
    if (condensedJson) {
      success = fromCondensedJson(doc);
    } else {
      success = false;
      // TODO implement from json and to json
      // success = fromJson(doc);
    }
  }
  Station(Uid Uid_, bool dontAdd_ = false) {
    Uid emptyUid;
    this->tag = Tag(Uid_, emptyUid, Point());
    include = dontAdd_;
  }

  bool fromCondensedJson(JsonObject& obj) {
    // Validate input
    try {
      if (obj.isNull()) {
        Serial.println("jsonToTrack: JSON object is null");
        return false;
      }

      // Track type

      // UID objects
      if (obj["tag"].is<JsonObject>()) {
        JsonObject tagObj = obj["uid"];
        tag.fromCondensedJson(tagObj);
      } else {
        return false;
      }

    } catch (const std::exception& e) {
      Serial.println("error");
      std::cerr << e.what() << '\n';
      return false;
    }

    return true;
  }
  void toCondensedJson(JsonObject& obj) const {
    JsonObject tagObj = obj["tag"].to<JsonObject>();
    tag.toCondensedJson(tagObj);
  }

  // Getter for tag
  Tag& getTag() { return tag; }
  const Tag& getTag() const { return tag; }

  // Setter for tag
  void setTag(Tag newTag) { tag = newTag; }

  // Getter for positioned
  bool getPositioned() const { return positioned; }

  // Setter for positioned
  void setPositioned(bool newStatus) { positioned = newStatus; }
  void setInclude(bool include_) { include = include_; }
  const bool getInclude() const { return include; }

  bool operator!=(const Station& other) const { return !(*this == other); }

  // Equality operator for comparing two Station objects
  bool operator==(const Station& other) const {
    if (tag != other.getTag()) return false;
    if (include != other.getInclude()) return false;
    if (positioned != other.getPositioned()) return false;
    return true;
  }

 private:
  Tag tag;                  // pointer to a Tag object
  bool positioned = false;  // boolean to indicate if the station is positioned
  bool include = true;
};

#endif  // STATION_HPP