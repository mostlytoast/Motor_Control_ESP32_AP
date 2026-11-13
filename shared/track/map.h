#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../utils/AppState.h"
#include "../utils/Math.hpp"
#include "../utils/uidLogic.h"
#include "Arduino.h"
#include "Station.hpp"
#include "Track.hpp"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
// TODO rename to be more like a map of objects since track contains stations
// now need to do rename for track.hpp too
class TrackMap {
 public:
  Track* createTrack(TrackType type, float length, float radius,
                     float branchAngle, float branchLength, float branchRadius,
                     bool direction, Uid entranceAUid, Uid exitAUid,
                     Uid entranceBUid = Uid(), Uid exitBUid = Uid(),
                     bool dontAdd = false, Uid connectEntranceAUid = Uid(),
                     Uid connectExitAUid = Uid(),
                     Uid connectEntranceBUid = Uid(),
                     Uid connectExitBUid = Uid());

  void addTrack(Track track);
  // void addStation(Station station);
  // Station* createStation(Uid stationUid, bool dontAdd);

  /*
   * Calculate the complete map.
   */
  void calculateMap();
  // bool connectStationToTag(Station* a, Tag* b);

  bool connectTags(Tag* a, Tag* b);
  const std::vector<Track>* getTracks() const;

  bool doUidsBelongToSameTrack(const Uid& uidA, const Uid& uidB);

  /*
   * returns true if track with associated uid is in track file
   */
  bool doesTrackExist(Uid uid);
  // auto* findObjectByUid(Uid uid);
  Track* findTrackByUid(Uid uid);
  // Station* findStationByUid(Uid uid);

  Tag* findTagByUid(Uid uid);

  uint8_t getTrackCount() const;

  void mapToJson(JsonDocument& doc);

  void JsonToMap(JsonArray doc);

  void mapToCondensedJson(JsonArray& addTracksJson,
                          // JsonArray& addStationsJson,
                          JsonArray& removeUidsFromMap);
  void condensedJsonToMap(JsonArray& addTracksJson,

                          // JsonArray& addStationsJson,
                          JsonArray& removeUidsFromMap);
  void clearMap();
  // bool removeStation(const Station* station);
  bool removeTrack(const Track* track);

  bool getMapModified();
  void setMapModified(bool mapModified_);

 private:
  bool mapModified = false;
  static constexpr uint8_t MAX_TRACKS = 100;
  // static constexpr uint8_t MAX_STATIONS = 100;

  // std::vector<Track> tracks;

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
   * Calculate a track when its entrance position and heading
   * are known.
   */
  void calculateFromEntrance(Track& track);

  /*
   * Curve geometry.
   */
  void calculateCurvePose(Track& track);

  /*
   * Switch geometry.
   */
  void calculateSwitchPose(Track& track);

  /*
   * Cross geometry.
   */
  void calculateCrossPose(Track& track);
};

extern TrackMap trackMap;