#ifndef TRACK_HPP
#define TRACK_HPP
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>

#include <iostream>
#include <string>

#include "../utils/uidLogic.h"
#include "Arduino.h"
#include "Point.hpp"
#include "Tag.hpp"

enum TrackType { STRAIGHT, CURVED, SWITCH, BUMPER, CROSS };
// todo make a clear track function?

// TODO add to string functions for everything (make this into a better
// structured class too)

class Track {
 public:
  Track() = default;
  Track(JsonObject obj, bool& success) {
    // ============================================================
    // Validate input
    // ============================================================
    success = false;

    if (obj.isNull()) {
      Serial.println("jsonToTrack: JSON object is null");
      return;
    }

    length = obj["length"].is<float>() ? obj["length"].as<float>() : 0;

    radius = obj["radius"].is<float>() ? obj["radius"].as<float>() : 0;

    direction =
        obj["direction"].is<bool>() ? obj["direction"].as<bool>() : false;

    branchAngle =
        obj["branchAngle"].is<float>() ? obj["branchAngle"].as<float>() : 0;

    branchRadius =
        obj["branchRadius"].is<float>() ? obj["branchRadius"].as<float>() : 0;

    branchLength =
        obj["branchLength"].is<float>() ? obj["branchLength"].as<float>() : 0;

    // ============================================================
    // Track type
    // ============================================================

    const char* type = obj["type"].as<const char*>();

    if (stringToTrackType(type, trackType)) return;

    // ============================================================
    // UID objects
    // ============================================================
    std::vector<Uid> Uids;
    std::vector<Uid> connectedUids;

    JsonArray tagsArray = obj["tags"].as<JsonArray>();

    if (!tagsArray.isNull()) {
      for (JsonObject tagObj : tagsArray) {
        const char* uidString = tagObj["uid"].as<const char*>();

        const char* connectedTagUid =
            tagObj["connectedTagUid"].as<const char*>();

        if (uidString == nullptr) {
          uidString = "";
        }
        Uid uid;
        uid.fromString(uidString);
        Uid connectedUid;
        connectedUid.fromString(connectedTagUid);
        tags.emplace_back(uid, connectedUid);
        // --------------------------------------------------------
        // Parse corresponding UID
        // --------------------------------------------------------
      }
    }

    // ============================================================
    // CREATE TRACK
    // ============================================================
    // todo might need to find way to add back in pose and position marker
    // settings?  dont want stateJson to call calculateMap?
    success = true;
  }

  Track(TrackType type, float length, float radius, float branchAngle,
        float branchRadius, float branchLength, bool direction,
        Uid entranceAUid, Uid exitAUid, Uid entranceBUid = Uid(),
        Uid exitBUid = Uid(), bool dontAdd = false,
        Uid connectEntranceAUid = Uid(), Uid connectExitAUid = Uid(),
        Uid connectEntranceBUid = Uid(), Uid connectExitBUid = Uid()) {
    this->length = length;
    this->radius = radius;
    this->branchAngle = branchAngle;
    this->branchRadius = branchRadius;
    this->branchLength = branchLength;
    this->direction = direction;
    this->trackType = type;
    this->include = !dontAdd;

    this->positioned = false;

    tags.emplace_back(entranceAUid, connectEntranceAUid);
    tags.emplace_back(exitAUid, connectExitAUid);
    tags.emplace_back(entranceBUid, connectEntranceBUid);
    tags.emplace_back(exitBUid, connectExitBUid);
    points.emplace_back(Point());
    points.emplace_back(Point());
    points.emplace_back(Point());
    points.emplace_back(Point());
  }

  /*
   * Calculate the complete map.
   */

  /*
   * returns true if track with associated uid is in track file
   */

  // float angle = 0;
  // float length = 0;
  // float radius = 0;
  // // for switch and cross
  // float branchAngle = 0;
  // float branchRadius = 0;
  // float branchLength = 0;

  void setAngle(float angle_) { angle = angle_; }
  float getAngle() { return angle; }

  void setLength(float length_) { length = length_; }
  float getLength() const { return length; }

  void setRadius(float radius_) { radius = radius_; }
  const float getRadius() const { return radius; }

  void setBranchAngle(float branchAngle_) { branchAngle = branchAngle_; }
  float getBranchAngle() { return branchAngle; }

  void setBranchRadius(float branchRadius_) { branchRadius = branchRadius_; }
  float getBranchRadius() const { return branchRadius; }

  void setBranchLength(float branchLength_) { branchLength = branchLength_; }
  const float getBranchLength() const { return branchLength; }

  void setPositioned(bool positioned_) { positioned = positioned_; }
  const bool getPositioned() const { return positioned; }

  void setDirection(bool direction_) { direction = direction_; }
  const bool getDirection() const { return direction; }

  void setInclude(bool include_) { include = include_; }
  const bool getInclude() const { return include; }

  void setTrackType(TrackType trackType_) { trackType = trackType_; }
  TrackType getTrackType() { return trackType; }

  const TrackType getTrackType() const { return trackType; }

  std::vector<Tag> getTags() { return tags; }
  bool operator!=(const Track& other) const { return !(*this == other); }

  // Equality operator for comparing two Track objects
  bool operator==(const Track& other) const {
    if (trackType != other.trackType) return false;
    if (length != other.length) return false;
    if (radius != other.radius) return false;
    if (branchAngle != other.branchAngle) return false;
    if (branchRadius != other.branchRadius) return false;
    if (branchLength != other.branchLength) return false;
    if (direction != other.direction) return false;
    if (include != other.include) return false;
    if (positioned != other.positioned) return false;

    // Compare tags
    if (tags.size() != other.tags.size()) return false;
    for (size_t i = 0; i < tags.size(); i++) {
      if (tags[i] != other.tags[i]) return false;
    }

    // Compare points
    if (points.size() != other.points.size()) return false;
    for (size_t i = 0; i < points.size(); i++) {
      if (points[i] != other.points[i]) return false;
    }

    return true;
  }

  const std::vector<Tag> getTags() const { return tags; }

  std::vector<Point> getPoints() { return points; }
  const std::vector<Point> getPoints() const { return points; }

  bool stringToTrackType(const char*& type, TrackType& trackType) {
    if (type == nullptr) {
      type = "STRAIGHT";
    }

    if (strcmp(type, "STRAIGHT") == 0) {
      trackType = STRAIGHT;

    } else if (strcmp(type, "CURVED") == 0) {
      trackType = CURVED;

    } else if (strcmp(type, "SWITCH") == 0) {
      trackType = SWITCH;

    } else if (strcmp(type, "BUMPER") == 0) {
      trackType = BUMPER;

    } else if (strcmp(type, "CROSS") == 0) {
      trackType = CROSS;

    } else {
      Serial.print("jsonToTrack: Unknown track type: ");

      Serial.println(type);

      return false;
    }
    return true;
  }

  void trackToJson(JsonObject& obj) {
    if (obj.isNull()) {
      Serial.println("jsonToTrack: JSON object is null");

      return;
    }
    obj["length"] = length;

    obj["radius"] = radius;

    obj["direction"] = direction;

    obj["branchAngle"] = branchAngle;

    obj["branchRadius"] = branchRadius;

    obj["branchLength"] = branchLength;

    obj["type"] = trackTypeToString(trackType);

    JsonObject pose = obj["pose"].to<JsonObject>();

    for (int i = 0; i < points.size(); i++) {
      pose[("X" + std::to_string(i)).c_str()] = points[i].getX();
      pose[("Y" + std::to_string(i)).c_str()] = points[i].getY();
      pose[("H" + std::to_string(i)).c_str()] = points[i].getHeading();
    }

    pose["positioned"] = positioned;

    JsonArray tagsArray = obj["tags"].to<JsonArray>();

    for (int i = 0; i < tags.size(); i++) {
      addTagJson(tagsArray, ("tag" + std::to_string(i)).c_str(), tags[i]);
    }
    // Serial.println("=== trackToJson ===");
    // serializeJsonPretty(obj, Serial);
  }
    /**
   * Prints the entire track structure to Serial using Pretty JSON formatting.
   * Call this using: track.printTrack();
   */
  void printTrack()  {
    JsonDocument doc;
    // Create the object structure
    JsonObject obj = doc.to<JsonObject>();
    
    // Fill the object with data
    trackToJson(obj);

    // Print to Serial
    serializeJsonPretty(doc, Serial);
    Serial.println("--- End of Track ---");
  }

  /**
   * Returns a simple string representation of the track for quick debugging.
   * Call this using: Serial.println(track.toString());
   */
  std::string toString() const {
    // printTrack();
    return "Track: " + std::string(trackTypeToString(trackType)) + 
           " | Len: " + std::to_string(length) + 
           " | Rad: " + std::to_string(radius) + 
           " | Dir: " + std::string(direction ? "CW" : "CCW");
  }



  const char* trackTypeToString() const { return trackTypeToString(trackType); }

  const char* trackTypeToString(TrackType type) const {
    switch (type) {
      case STRAIGHT:
        return "STRAIGHT";

      case CURVED:
        return "CURVED";

      case SWITCH:
        return "SWITCH";

      case BUMPER:
        return "BUMPER";

      case CROSS:
        return "CROSS";

      default:
        return "UNKNOWN";
    }
  }

 private:
  void addTagJson(JsonArray& array, const char* name, Tag& tag) {
    JsonObject obj = array.add<JsonObject>();

    obj["name"] = name;

    // Uid now knows its own length.
    obj["uid"] = tag.getUid().toString();

    // Store whether the UID is valid.
    obj["valid"] = tag.getUid().isValid();

    Uid emptyUid;
    obj["connectedTagUid"] = tag.getConnectedTagUid().toString();
  }

  // TOdo might want to have these included as offset values in points?
  float angle = 0;
  float length = 0;
  float radius = 0;
  // for switch and cross
  float branchAngle = 0;
  float branchRadius = 0;
  float branchLength = 0;

  bool include = true;
  /*
   * Direction of a curve.
   *
   * This follows the coordinate convention used by the original
   * map.cpp implementation:
   *
   *     true  = clockwise
   *     false = counter-clockwise
   */

  //  todo remove direction just use an angle measure?
  bool direction = false;

  bool positioned = false;
  TrackType trackType = STRAIGHT;
  // TODO might want to store in array/dictionary?

  std::vector<Tag> tags;

  /*
   * Position in the global track map.
   */
  std::vector<Point> points;
};

#endif  // TRACK_H
