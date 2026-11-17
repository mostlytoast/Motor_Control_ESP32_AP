#include "transmitter.h"

// TODO add usefull debug print lines througout code

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  delay(500);

  Serial.println();

  Serial.println("ESP32 CYD Multi-Receiver Motor Controller");
  display.setupDisplay();
  setupEncoder();
  // Uid entrance0("AA:AA:AA:AA:AA:AA:A0");
  // Uid exit0("AA:AA:AA:AA:AA:AA:B0");
  // Uid entrance1("AA:AA:AA:AA:AA:AA:A1");
  // Uid exit1("AA:AA:AA:AA:AA:AA:B1");
  // Uid entrance2("AA:AA:AA:AA:AA:AA:A2");
  // Uid exit2("AA:AA:AA:AA:AA:AA:B2");
  // Uid entrance3("AA:AA:AA:AA:AA:AA:A3");
  // Uid exit3("AA:AA:AA:AA:AA:AA:B3");
  // Track* trackA = trackMap.createTrack(STRAIGHT, 10, 0, 0, 0, 0, true, entrance0, exit0);

  // Track* trackB = trackMap.createTrack(STRAIGHT, 10, 0, 0, 0, 0, true, entrance0, exit0);

  // Track* trackC = trackMap.createTrack(STRAIGHT, 1, 0, 0, 0, 0, true, entrance0, exit0);
  // Serial.println("trackA == trackB");
  // Serial.println(*trackA == *trackB);
  // Serial.println("trackA == trackC");
  // Serial.println(*trackA == *trackC);

  // trackMap.createTrack(CURVED, 18, 14, 0, 0, 0, true, entrance1, exit1);
  // trackMap.connectUid(exit0, entrance1);
  // trackMap.calculateMap();
  // convertToAndFromJson();
  // trackMap.createTrack(STRAIGHT, 10, 0, 0, 0, 0, true, entrance2, exit2);
  // trackMap.createTrack(CURVED, 18, 14, 0, 0, 0, true, entrance3, exit3);

  // trackMap.connectUid(exit1, entrance2);
  // trackMap.connectUid(exit2, entrance3);
  // trackMap.calculateMap();

  // convertToAndFromJson();
}

void convertToAndFromJson() {
  JsonDocument doc1;
  JsonArray addTracks1 = doc1["addTracks"].to<JsonArray>();
  // JsonArray addStations = doc1["addStations"].to<JsonArray>();
  JsonArray removeUid1 = doc1["removeUid"].to<JsonArray>();
  trackMap.mapToCondensedJson(addTracks1, removeUid1);
   Serial.println(
      "=========================== output ===========================");
  serializeJsonPretty(doc1, Serial);

  Serial.println(
      "=========================== before ===========================");
  JsonDocument docA;

  trackMap.mapToJson(docA);
  serializeJsonPretty(docA, Serial);
  // trackMap.clearMap();
  trackMap.condensedJsonToMap(addTracks1, removeUid1);

  Serial.println(
      "=========================== after ===========================");
  JsonDocument docB;

  trackMap.mapToJson(docB);
  serializeJsonPretty(docB, Serial);
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  updateEncoder();

  display.refresh();
}
