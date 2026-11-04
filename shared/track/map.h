#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../utils/uidLogic.h"
#include "Arduino.h"
#include "Station.hpp"
#include "Track.hpp"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TrackMap {
 public:
  Track* createTrack(TrackType type, float length, float radius,
                   float branchAngle, float branchRadius, float branchLength,
                   bool direction, Uid entranceAUid, Uid exitAUid,
                   Uid entranceBUid = Uid(), Uid exitBUid = Uid(),
                   bool dontAdd = false, Uid connectEntranceAUid = Uid(),
                   Uid connectExitAUid = Uid(), Uid connectEntranceBUid = Uid(),
                   Uid connectExitBUid = Uid());
  void addTrack(Track track);

  // Station* createStation(Track* track, Uid stationUID, bool dontAdd = false);
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

  int trackIndex(const Track* track) const;

  void mapToJson(JsonDocument& doc);

  void JsonToMap(JsonArray doc);


 private:
  static constexpr uint8_t MAX_TRACKS = 100;

  std::vector<Track> tracks;


  //  used when creating track that should not be added
  Track tempTrack;


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

  void clearMap();
};

extern TrackMap trackMap;