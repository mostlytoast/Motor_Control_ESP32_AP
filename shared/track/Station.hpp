
#ifndef STATION_HPP
#define STATION_HPP
#include "Point.hpp"
#include "Tag.hpp"

class Station {
 public:
 public:  // Constructor
  Station() = default;
  Station(Tag tag_) { this->tag = tag_; }
  // Getter for tag
  Tag& getTag() { return tag; }

  // Setter for tag
  void setTag(Tag newTag) { tag = newTag; }

  // Getter for positioned
  bool isPositioned() const { return positioned; }

  // Setter for positioned
  void setPositioned(bool newStatus) { positioned = newStatus; }

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

 private:
  Tag tag;                  // pointer to a Tag object
  bool positioned = false;  // boolean to indicate if the station is positioned
};

#endif  // STATION_HPP