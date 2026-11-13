#ifndef TRACK_HPP
#define TRACK_HPP
#include <ArduinoJson.h>

#include <iostream>
#include <string>

#include "../utils/Math.hpp"
#include "../utils/uidLogic.h"
#include "Arduino.h"
#include "Point.hpp"
#include "Tag.hpp"

enum TrackType { STRAIGHT, CURVED, SWITCH, BUMPER, CROSS, STATION };

// TODO make it so there is only Straight, Curved, and bumper then when creating
// things like a switch or cross you just combine the concepts into one (do
// before auto driving)
class Track {
 public:
  Track() = default;
  Track(bool& success, JsonObject doc, bool condensedJson = false) {
    if (condensedJson) {
      success = fromCondensedJson(doc);
    } else {
      success = fromJson(doc);
    }
  }
  bool fromJson(JsonObject& obj) {
    // Validate input

    if (obj.isNull()) {
      Serial.println("jsonToTrack: JSON object is null");
      return false;
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

    // Track type

    trackType = obj["t"].as<TrackType>();
    // UID objects
    JsonArray tagsArray = obj["tags"].as<JsonArray>();

    if (!tagsArray.isNull()) {
      for (JsonObject tagObj : tagsArray) {
        bool success;
        tags.emplace_back(success, tagObj);
        if (!success) {
          return false;
        }
      }
    }

    if (tags.size() == 0) {
      return false;
    }

    // todo might need to find way to add back in pose and position marker
    // settings?  dont want stateJson to call calculateMap?

    return setTagPositions();
  }

  void toJson(JsonObject& obj) const {
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

    obj["positioned"] = positioned;

    JsonArray tagsArray = obj["tags"].to<JsonArray>();

    for (int i = 0; i < tags.size(); i++) {
      JsonObject obj = tagsArray.add<JsonObject>();
      tags[i].toJson(obj);
    }
  }
  /**
   * Prints the entire track structure to Serial using Pretty JSON formatting.
   * Call this using: track.printTrack();
   */
  void printTrack() {
    JsonDocument doc;
    // Create the object structure
    JsonObject obj = doc.to<JsonObject>();

    // Fill the object with data
    toJson(obj);

    // Print to Serial
    serializeJsonPretty(doc, Serial);
    Serial.println("--- End of Track ---");
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
    tags.emplace_back(entranceAUid, connectEntranceAUid, Point());
    tags.emplace_back(exitAUid, connectExitAUid, Point());
    tags.emplace_back(entranceBUid, connectEntranceBUid, Point());
    tags.emplace_back(exitBUid, connectExitBUid, Point());
    setTagPositions();
  }

  bool fromCondensedJson(JsonObject& obj) {
    // Validate input
    Serial.println("track");
    try {
      if (obj.isNull()) {
        Serial.println("jsonToTrack: JSON object is null");
        return false;
      }

      length = obj["l"].is<float>() ? obj["l"].as<float>() : 0;

      radius = obj["r"].is<float>() ? obj["r"].as<float>() : 0;

      direction = obj["d"].is<bool>() ? obj["d"].as<bool>() : false;

      branchAngle = obj["a"].is<float>() ? obj["a"].as<float>() : 0;

      branchRadius = obj["h"].is<float>() ? obj["h"].as<float>() : 0;

      branchLength = obj["o"].is<float>() ? obj["o"].as<float>() : 0;

      // Track type

      trackType = obj["t"].as<TrackType>();
      // UID objects

      JsonArray tagsArray = obj["g"].as<JsonArray>();
      // Serial.println("track1");

      if (!tagsArray.isNull()) {
        for (JsonObject tagObj : tagsArray) {
          Tag tag;
          // Serial.println("track2");

          if (tag.fromCondensedJson(tagObj)) {
            tags.emplace_back(tag);
          }
        }
      }
      // Serial.println("track3");

      if (tags.size() == 0) {
        return false;
      }

      // todo might need to find way to add back in pose and position marker
      // settings?  dont want stateJson to call calculateMap?
      return setTagPositions();

    } catch (const std::exception& e) {
      Serial.println("error");
      std::cerr << e.what() << '\n';
      return false;
    }

    return true;
  }

  void toCondensedJson(JsonObject& obj) const {
    obj["l"] = length;

    obj["r"] = radius;

    obj["d"] = direction;

    obj["a"] = branchAngle;

    obj["h"] = branchRadius;

    obj["o"] = branchLength;

    obj["t"] = trackType;

    JsonArray tagsArray = obj["g"].to<JsonArray>();

    for (int i = 0; i < tags.size(); i++) {
      JsonObject obj = tagsArray.add<JsonObject>();
      tags[i].toCondensedJson(obj);
    }
  }

  void setAngle(float angle_) { angle = angle_; }
  float getAngle() { return angle; }

  void setLength(float length_) { length = length_; }
  float getLength() const { return length; }

  void setRadius(float radius_) { radius = radius_; }
  const float getRadius() const { return radius; }

  void setSectionsAndRadius(float sections_, float radius_) {
    radius = radius_;
    length = (2 * radius_ * M_PI) / sections_;
  }

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

  std::vector<Tag>& getTags() { return tags; }

  const std::vector<Tag>& getTags() const { return tags; }
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

    return true;
  }

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

    } else if (strcmp(type, "STATION") == 0) {
      trackType = STATION;

    } else {
      Serial.print("jsonToTrack: Unknown track type: ");

      Serial.println(type);

      return false;
    }
    return true;
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
      case STATION:
        return "STATION";

      default:
        return "UNKNOWN";
    }
  }

  /**
   * @brief position the unpositionfd track by using the source tag as a
   * starting point to manipulate the destinationTag
   *
   * @param sourceTag
   * @param destinationTag
   */
  void calculatePoseFromSourceTag(Tag& sourceTag, Tag* destinationTag) {
    // TODO find way to make station be positioned relative to track?
    if (destinationTag == nullptr) return;

    const Point originalDestination = destinationTag->getPoint();
    const Point target = sourceTag.getPoint();

    // Outward-facing headings must oppose each other.
    float rotation =
        (target.getHeading() + 180.0f) - originalDestination.getHeading();

    while (rotation >= 360.0f) rotation -= 360.0f;
    while (rotation < 0.0f) rotation += 360.0f;

    for (Tag& tag : tags) {
      tag.getPoint().transformPoint(originalDestination, target, rotation);
    }

    positioned = true;
  }

 private:
  // todo maybe move to a math file

  void setCurveOrStraightPose(Tag& entranceTag, Tag& exitTag, float length_,
                              float radius_) {
    entranceTag.addInternalConnectedTagUid(exitTag.getUid());
    exitTag.addInternalConnectedTagUid(entranceTag.getUid());
    if (radius_ == 0.0f) {
      entranceTag.setPoint(Point(0.0f, 0.0f, 180.0f));
      exitTag.setPoint(Point(length_, 0.0f, 0.0f));
      return;
    }

    entranceTag.setPoint(Point(0.0f, 0.0f, 180.0f));
    entranceTag.getPoint().normalizeHeading();

    float turn = direction ? -radiansToDegrees(length_ / radius_)
                           : radiansToDegrees(length_ / radius_);
    if (std::isinf(turn)) {
      Serial.println("heading is infinite");
      turn = 0.0;
      return;
    }
    const float centerHeading = (entranceTag.getPoint().getHeading() - 180) +
                                (direction ? -90.0f : 90.0f);

    Point center = entranceTag.getPoint();

    center.movePoint(centerHeading, radius_);

    const float startAngle =
        atan2(entranceTag.getPoint().getY() - center.getY(),
              entranceTag.getPoint().getX() - center.getX());

    const float endAngle = startAngle + degreesToRadians(turn);

    exitTag.setPoint(Point(center.getX() + cos(endAngle) * radius_,
                           center.getY() + sin(endAngle) * radius_,
                           exitTag.getPoint().getHeading() + turn));
    exitTag.getPoint().normalizeHeading();
  }

  bool setTagPositions() {
    // Serial.println("track5");

    switch (trackType) {
      case BUMPER:
      case STRAIGHT:

        // Serial.println("track6");

        if (tags.size() < 2) return false;
        // Serial.println("track6a");

        setCurveOrStraightPose(tags[0], tags[1], length, 0);
        break;
      case CURVED:
        // Serial.println("track7");

        if (tags.size() < 2) return false;
        // Serial.println("track7a");

        setCurveOrStraightPose(tags[0], tags[1], length, radius);
        // tags[2].getPoint().setHeading(0.0f);

        break;

      case SWITCH:
        if (tags.size() < 4) return false;

        setCurveOrStraightPose(tags[0], tags[1], length, radius);

        setCurveOrStraightPose(tags[2], tags[3], branchLength, branchRadius);
        tags[0].addInternalConnectedTagUid(tags[2].getUid());
        tags[2].addInternalConnectedTagUid(tags[0].getUid());
        break;

      case CROSS: {
        if (tags.size() < 4) return false;

        setCurveOrStraightPose(tags[0], tags[1], length, radius);
        setCurveOrStraightPose(tags[2], tags[3], branchLength, 0);

        Serial.print("branchLength ");
        Serial.print(branchLength);
        Serial.print(" branchRadius ");
        Serial.println(branchRadius);

        Point entrance = tags[0].getPoint();
        Point exit = tags[1].getPoint();

        Point halfwayPoint((entrance.getX() + exit.getX()) / 2.0f,
                           (entrance.getY() + exit.getY()) / 2.0f,
                           entrance.getHeading());

        tags[2].getPoint().rotatePoint(30, halfwayPoint);
        tags[3].getPoint().rotatePoint(30, halfwayPoint);

        break;
      }

      case STATION:
        if (tags.size() < 1) return false;
        tags[0].setPoint(Point(0.0f, 0.0f, 180.0f));
        break;

      default:
        Serial.println("wrong type");

        return false;
    }
    return true;
  }

  // TOdo might want to have these included as offset values in points?
  float angle = 0.0;
  float length = 0.0;
  float radius = 0.0;
  // for switch and cross
  float branchAngle = 0.0;
  float branchRadius = 0.0;
  float branchLength = 0.0;

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
};

#endif  // TRACK_H
