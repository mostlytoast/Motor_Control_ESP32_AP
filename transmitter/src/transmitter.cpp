#include "transmitter.h"

// TODO add usefull debug print lines througout code

// =====================================================
// SETUP
// =====================================================
Uid pastUid;
bool hasPastUid = false;

void addAndConnectTrack(TrackType type, float length, float radius,
                        float branchAngle, float branchLength,
                        float branchRadius, bool direction) {
  Uid uidA;
  uidA.random();
  Uid uidB;
  uidB.random();
  trackMap.createTrack(type, length, radius, branchAngle, branchLength,
                       branchRadius, direction, uidA, uidB);
  if (!hasPastUid) {
    pastUid = uidB;
    hasPastUid = true;
  } else {
    trackMap.connectUid(pastUid, uidB);
  }
}
void setup() {
  Serial.begin(115200);

  delay(500);

  Serial.println();

  Serial.println("ESP32 CYD Multi-Receiver Motor Controller");
  display.setupDisplay();
  setupEncoder();

  // Uid entrance0("AA:AA:AA:AA:AA:AA:A0");
  // Uid exit0("AA:AA:AA:AA:AA:AA:B0");
  // trackMap.createTrack(STRAIGHT, 10, 0, 0, 0, 0, true, entrance0, exit0);

  // Uid entrance1("AA:AA:AA:AA:AA:AA:A1");
  // Uid exit1("AA:AA:AA:AA:AA:AA:B1");

  // trackMap.createTrack(CURVED, 10, 19, 0, 0, 0, true, entrance1, exit1);
  // Uid entrance2("AA:AA:AA:AA:AA:AA:A2");
  // Uid exit2("AA:AA:AA:AA:AA:AA:B2");

  // trackMap.createTrack(STATION, 10, 0, 0, 0, 0, true, entrance2, exit2);
  
  // Uid entrance3("AA:AA:AA:AA:AA:AA:A3");
  // Uid exit3("AA:AA:AA:AA:AA:AA:B3");
  // trackMap.createTrack(CURVED, 18, 14, 0, 0, 0, true, entrance3, exit3);

  // trackMap.connectUid(exit0, entrance1);
  // trackMap.connectUid(entrance1, entrance2);

  // // trackMap.connectUid(exit1, entrance2);
  // trackMap.connectUid(exit1, entrance3);
  // trackMap.calculateMap();
  // app.setDestinationUid(entrance2);
  // app.setCurrentUid(entrance0);

  // // addAndConnectTrack(STRAIGHT, 10, 0, 0, 0, 0, true);
  // // addAndConnectTrack(CURVED, 18, 14, 0, 0, 0, true);
  // // addAndConnectTrack(CURVED, 18, 14, 0, 0, 0, true);
  // // addAndConnectTrack(CURVED, 18, 14, 0, 0, 0, true);
  // // addAndConnectTrack(STRAIGHT, 10, 0, 0, 0, 0, true);
  // // addAndConnectTrack(STRAIGHT, 10, 0, 0, 0, 0, true);
  // // addAndConnectTrack(STRAIGHT, 10, 0, 0, 0, 0, true);
  // // addAndConnectTrack(STRAIGHT, 10, 0, 0, 0, 0, true);
  // // addAndConnectTrack(SWITCH, 18, 0, 0, 18, 14, true);
  // // addAndConnectTrack(STRAIGHT, 10, 0, 0, 0, 0, true);
  // // addAndConnectTrack(STRAIGHT, 10, 0, 0, 0, 0, true);

  // trackMap.calculateMap();
  // app.setCurrentUid(
  //     app.getCurrentTracks()->operator[](0).getTags().operator[](0).getUid());

  // app.setDestinationUid(
  //     app.getCurrentTracks()->operator[](8).getTags().operator[](0).getUid());
  // // const Tracks* tracks = app.getCurrentTracks();
  // TrackPathFinder pathFinder(*app.getCurrentTracks());
  // // pathFinder(*app.getCurrentTracks());
  // bool result = pathFinder.findPath(app.getCurrentUid(),
  //                                   app.getDestinationUid(), app.getPath());
  // Serial.println(result);

  // convertToAndFromJson();
}

// void convertToAndFromJson() {
//   JsonDocument doc1;
//   JsonArray addTracks1 = doc1["addTracks"].to<JsonArray>();
//   // JsonArray addStations = doc1["addStations"].to<JsonArray>();
//   JsonArray removeUid1 = doc1["removeUid"].to<JsonArray>();
//   trackMap.mapToCondensedJson(addTracks1, removeUid1);
//   Serial.println(
//       "=========================== output ===========================");
//   serializeJsonPretty(doc1, Serial);

//   Serial.println(
//       "=========================== before ===========================");
//   JsonDocument docA;

//   trackMap.mapToJson(docA);
//   serializeJsonPretty(docA, Serial);
//   // trackMap.clearMap();
//   trackMap.condensedJsonToMap(addTracks1, removeUid1);

//   Serial.println(
//       "=========================== after ===========================");
//   JsonDocument docB;

//   trackMap.mapToJson(docB);
//   serializeJsonPretty(docB, Serial);
// }

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  updateEncoder();
  display.refresh();
}
