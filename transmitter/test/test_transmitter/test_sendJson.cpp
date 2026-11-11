#include <ArduinoJson.h>
#include <unity.h>

#include <string>
#include <vector>

#include "../../../shared/utils/AppState.h"
#include "../../../shared/utils/StateJson.h"


void setupTestApp() {
  // Reset your app to a known baseline
  app.reset();
}

// --- Test Case: Standard JSON Round Trip ---
void test_StandardJsonRoundTrip() {
  setupTestApp();

  // 1. Set Initial State
  app.setScannerMode(ScannerMode::REGISTER_TRACKS);  // Assuming an enum
  app.setLed1State(true);
  app.setLed2State(false);
  app.setAutoState(true);
  app.setUserSpeed(500);
  app.setRampTime(1500);  // 1.5 seconds
  app.setAudioState(true);
  app.setStatusLabelText("Test Status");

  // 2. Convert State -> JSON
  JsonDocument doc;
  getJsonFromState(doc);

  // Verify JSON structure (Optional but good for debugging)
  TEST_ASSERT_TRUE(doc["state"].isNull() == false);
  TEST_ASSERT_EQUAL(true, doc["state"]["led1"].as<bool>());
  TEST_ASSERT_EQUAL(500, doc["state"]["speed"].as<int>());

  // 3. Convert JSON -> State
  getStateFromJson(doc);

  // 4. Assertions
  TEST_ASSERT_EQUAL(ScannerMode::REGISTER_TRACKS, app.getScannerMode());
  TEST_ASSERT_EQUAL(true, app.getLed1State());
  TEST_ASSERT_EQUAL(false, app.getLed2State());
  TEST_ASSERT_EQUAL(true, app.getAutoState());
  TEST_ASSERT_EQUAL(500, app.getUserSpeed());
  TEST_ASSERT_EQUAL(1500, app.getRampTime());
  TEST_ASSERT_EQUAL(true, app.getAudioState());
  // TEST_ASSERT_EQUAL(std::string("Test Status"), app.getStatusLabelText());

  printf("Standard Round Trip Passed!\n");
}

// --- Test Case: Condensed JSON Round Trip ---
void test_CondensedJsonRoundTrip() {
  setupTestApp();

  // 1. Set Initial State
  app.setScannerMode(ScannerMode::REGISTER_TRACKS);
  app.setLed1State(false);
  app.setLed2State(true);
  app.setAutoState(false);
  app.setUserSpeed(250);
  app.setRampTime(0);
  app.setAudioState(false);
  app.setScannerStatusLabelText("System_Ready");

  // 2. Convert State -> Condensed JSON
  JsonDocument doc;
  getCondensedJsonFromState(doc);

  // Verify Condensed Keys (s, 1, 2, a, p, r, d)
  TEST_ASSERT_EQUAL(false, doc["state"]["1"].as<bool>());  // led1
  TEST_ASSERT_EQUAL(250, doc["state"]["p"].as<int>());     // speed

  // 3. Convert Condensed JSON -> State
  getStateFromCondensedJson(doc);

  // 4. Assertions
  TEST_ASSERT_EQUAL(ScannerMode::REGISTER_TRACKS, app.getScannerMode());
  TEST_ASSERT_EQUAL(false, app.getLed1State());
  TEST_ASSERT_EQUAL(true, app.getLed2State());
  TEST_ASSERT_EQUAL(false, app.getAutoState());
  TEST_ASSERT_EQUAL(250, app.getUserSpeed());
  TEST_ASSERT_EQUAL(0, app.getRampTime());
  TEST_ASSERT_EQUAL(false, app.getAudioState());
  // TEST_ASSERT_EQUAL(std::string("System_Ready"),
  //                   app.getScannerStatusLabelText());

  printf("Condensed Round Trip Passed!\n");
}

// --- Test Case: Edge Case - Zero and Negative Values ---
void test_RampEdgeCases() {
  setupTestApp();

  // Test negative ramp (should be clamped to 0)
  app.setRampTime(1000);

  JsonDocument doc;
  getJsonFromState(doc);
  getStateFromJson(doc);

  // The code says: if (rampSeconds < 0.0f) { rampSeconds = 0.0f; }
  // However, since we set 1000, let's test a forced negative if possible
  // This tests the logic inside getStateFromJson

  // REGISTER_TRACKSly inject a negative value into the JSON document
  doc["state"]["ramp"] = -500.0f;
  getStateFromJson(doc);

  TEST_ASSERT_EQUAL(0, app.getRampTime());
  printf("Ramp Edge Case Passed!\n");
}

// int main() {
//   UNITY_BEGIN();
//   RUN_TEST(test_StandardJsonRoundTrip);
//   RUN_TEST(test_CondensedJsonRoundTrip);
//   RUN_TEST(test_RampEdgeCases);
//   UNITY_END();
//   return 0;
// }
