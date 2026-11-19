#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

extern Preferences preferences;

class SavePrefs {
 public:
  bool save(JsonDocument prefs) {
    String prefsString;
    serializeJson(prefs, prefsString);
    preferences.begin("data", false);  // false = read/write mode
    preferences.putString("config", prefsString);
    preferences.end();

    return true;
  }
  bool load(JsonDocument& prefs) {
    preferences.begin("data", true);  // true = read-only mode
    String jsonString =
        preferences.getString("config", "{}");  // Default to empty JSON
    preferences.end();

    // 2. Deserialize the String back into a JSON document
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
      Serial.print("JSON parse failed: ");
      Serial.println(error.c_str());
      return false;
    }
    prefs = doc;

    return true;
  }
};