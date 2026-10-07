#pragma once
#include "Arduino.h"
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../utils/uidLogic.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum TrackType { STRAIGHT, CURVED, SWITCH, BUMPER, CROSS };
// todo make a clear track function?
struct Track;
// TODO add to string functions for everything (make this into a better
// structured class too)
struct Tag {
  Uid uid;
  Track* track = nullptr;
  Tag* connectedTag = nullptr;
};

struct Point {
  float x;
  float y;

  Point() : x(0.0f), y(0.0f) {}
  Point(float x_, float y_) : x(x_), y(y_) {}
};

struct TrackPose {
  Point entrance;
  Point exit;

  Point entranceB;
  Point exitB;

  // Direction of travel at the entrance and exit.
  //
  // entranceAHeading:
  //     direction the train travels INTO the track
  //
  // exitAHeading:
  //     direction the train travels OUT OF the track
  //   TODO maybe move into track?
  float entranceAHeading = 0;
  float exitAHeading = 0;
  float entranceBHeading = 0;
  float exitBHeading = 0;
};

struct Track {
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

  TrackType trackType = STRAIGHT;
  // TODO might want to store in array/dictionary?
  Tag entranceA;
  Tag entranceB;
  Tag exitA;
  Tag exitB;

  /*
   * Position in the global track map.
   */
  TrackPose pose;

  bool positioned = false;
};

class TrackMap {
 public:
  Track* createTrack(TrackType type, float length, float radius,
                     float branchAngle, float branchRadius, float branchLength,
                     bool direction, Uid entranceAUid, Uid exitAUid,
                     Uid entranceBUid = Uid(), Uid exitBUid = Uid(),
                     bool dontAdd = false);

  /*
   * Calculate the complete map.
   */
  void calculateMap();

  bool connectTags(Tag* a, Tag* b);

  Track* getTrack(uint8_t index);

  bool doUidsBelongToSameTrack(const Uid& uidA, const Uid& uidB);

  /*
   * returns true if track with associated uid is in track file
   */
  bool doesTrackExist(Uid uid);

  Track* findTrackByUid(Uid uid);

  Tag* findTagByUid(Uid uid);

  uint8_t getTrackCount() const;

  void mapToJson(JsonDocument& doc);

  Track* jsonToTrack(JsonObject obj);

  void trackToJson(Track& track, JsonObject& ojb);

  void printTrack(Track& track);

 private:
  static constexpr uint8_t MAX_TRACKS = 100;

  Track tracks[MAX_TRACKS];
  uint8_t trackCount = 0;
  //  used when creating track that should not be added
  Track tempTrack;
  const char* trackTypeToString(TrackType type);

  void addTagJson(JsonArray& array, const char* name, Tag& tag);

  /*
   * Position a track using an already-positioned connected track.
   *
   * This supports connections to BOTH entrances and exits.
   */
  bool positionConnectedTrack(Track& source, Tag& sourceTag);

  /*
   * Straight-track geometry.
   */
  void calculateStraightPose(Track& track);

  /*
   * Calculate a straight track when its exit position
   * and heading are known.
   */
  void calculateStraightFromExit(Track& track);

  /*
   * Calculate a track when its entrance position and heading
   * are known.
   */
  void calculateFromEntrance(Track& track);

  /*
   * Calculate a track when its exit position and heading
   * are known.
   */
  void calculateFromExit(Track& track);

  /*
   * Original/general track pose calculation.
   */
  void calculateTrackPose(Track& track);

  /*
   * Curve geometry.
   */
  void calculateCurvePose(Track& track);

  /*
   * Calculate a curve when its exit position and heading
   * are known.
   */
  void calculateCurveFromExit(Track& track);

  /*
   * Switch geometry.
   */
  void calculateSwitchPose(Track& track);

  /*
   * Cross geometry.
   */
  void calculateCrossPose(Track& track);

  int findTrackIndex(Track* track);
};