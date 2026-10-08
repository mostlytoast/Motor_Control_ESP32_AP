#include "ControllerStorage.h"
Preferences preferences;

// =====================================================
// SAVE CONTROLLERS
// =====================================================

void saveControllers() {
  preferences.begin("controllers", false);

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    char key[8];

    snprintf(key, sizeof(key), "mac%d", i);

    if (app.getControllers()[i].valid) {
      preferences.putBytes(key, app.getControllers()[i].mac, 6);

      char nameKey[8];

      snprintf(nameKey, sizeof(nameKey), "name%d", i);

      preferences.putString(nameKey, app.getControllers()[i].name);

    }

    else {
      preferences.remove(key);

      char nameKey[8];

      snprintf(nameKey, sizeof(nameKey), "name%d", i);

      preferences.remove(nameKey);
    }
  }

  preferences.end();
}

void loadControllers() {
  memset(app.getControllers(), 0, sizeof(app.getControllers()));

  if (!preferences.begin("controllers", true)) {
    Serial.println("Failed to open controller preferences");

    return;
  }

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    char key[8];

    snprintf(key, sizeof(key), "mac%d", i);

    if (preferences.getBytesLength(key) == 6) {
      preferences.getBytes(key, app.getControllers()[i].mac, 6);

      char nameKey[8];

      snprintf(nameKey, sizeof(nameKey), "name%d", i);

      String name = preferences.getString(nameKey, "Controller");

      strncpy(app.getControllers()[i].name, name.c_str(),
              sizeof(app.getControllers()[i].name) - 1);

      app.getControllers()[i].name[sizeof(app.getControllers()[i].name) - 1] = '\0';

      app.getControllers()[i].valid = true;
    }
  }

  preferences.end();
}
