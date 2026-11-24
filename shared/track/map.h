#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../SharedConfig.h"
#include "../utils/AppState.hpp"
#include "../utils/Math.hpp"
#include "../utils/Uid.hpp"
#include "Arduino.h"
#include "Tracks.hpp"
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

  /*
   * Calculate the complete map.
   */
  void calculateMap();
  // bool connectStationToTag(Station* a, Tag* b);
  bool connectUid(Uid a, Uid b);
  bool connectTags(Tag* a, Tag* b);

  void mapToJson(JsonDocument& doc);

  void jsonToMap(JsonArray doc);

  void mapToCondensedJson(JsonDocument& doc);
  void condensedJsonToMap(JsonArray doc);
  void clearMap();

 private:
  //  used when creating track that should not be added
  Track tempTrack;

  /*
   * Position a track using an already-positioned connected track.
   *
   * This supports connections to BOTH entrances and exits.
   */
  bool positionConnectedTrack(Track& source, Tag& sourceTag);
};

extern TrackMap trackMap;