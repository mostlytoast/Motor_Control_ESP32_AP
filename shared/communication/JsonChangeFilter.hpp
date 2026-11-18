
#pragma once

#include <ArduinoJson.h>
#include <string.h>
class JsonChangeFilter {
 public:
  // Compares a new state against the previous state.
  // Returns only the changed values.
  static JsonDocument createDelta(JsonVariantConst previous,
                                  JsonVariantConst current) {
    JsonDocument delta;

    if (!previous.is<JsonObjectConst>() || !current.is<JsonObjectConst>()) {
      return delta;
    }

    JsonObjectConst oldObj = previous.as<JsonObjectConst>();
    JsonObjectConst newObj = current.as<JsonObjectConst>();

    JsonObject changes = delta.to<JsonObject>();

    for (JsonPairConst pair : newObj) {
      const char* key = pair.key().c_str();

      JsonVariantConst oldValue = oldObj[key];
      JsonVariantConst newValue = pair.value();

      // New key: send the entire value.
      if (oldValue.isUnbound()) {
        changes[key].set(newValue);
        continue;
      }

      // Arrays: only send changed elements.
      if (newValue.is<JsonArrayConst>() && oldValue.is<JsonArrayConst>()) {
        JsonArrayConst oldArray = oldValue.as<JsonArrayConst>();

        JsonArrayConst newArray = newValue.as<JsonArrayConst>();

        JsonObject arrayChanges = changes[key].to<JsonObject>();

        bool changed = false;

        for (size_t i = 0; i < newArray.size(); i++) {
          // New array element.
          if (i >= oldArray.size()) {
            arrayChanges[String(i)].set(newArray[i]);
            changed = true;
            continue;
          }

          // Existing element changed.
          if (newArray[i] != oldArray[i]) {
            arrayChanges[String(i)].set(newArray[i]);
            changed = true;
          }
        }

        // Array was shortened.
        if (newArray.size() < oldArray.size()) {
          arrayChanges["_size"] = newArray.size();
          changed = true;
        }

        if (!changed) {
          changes.remove(key);
        }

        continue;
      }

      // Nested objects: recursively compare.
      if (newValue.is<JsonObjectConst>() && oldValue.is<JsonObjectConst>()) {
        JsonDocument nestedDelta = createDelta(oldValue, newValue);

        if (!nestedDelta.as<JsonObject>().isNull() &&
            !nestedDelta.as<JsonObject>().size()) {
          continue;
        }

        if (nestedDelta.as<JsonObject>().size() > 0) {
          changes[key].set(nestedDelta.as<JsonObject>());
        }

        continue;
      }

      // Everything else: compare and send whole value.
      if (newValue != oldValue) {
        changes[key].set(newValue);
      }
    }

    // Handle removed top-level keys.
    for (JsonPairConst pair : oldObj) {
      const char* key = pair.key().c_str();

      if (newObj[key].isUnbound()) {
        changes[key]["_deleted"] = true;
      }
    }

    return delta;
  }

  // Applies a delta to an existing state.
  static void applyDelta(JsonVariant destination, JsonVariantConst delta) {
    if (!destination.is<JsonObject>() || !delta.is<JsonObjectConst>()) {
      return;
    }

    JsonObject target = destination.as<JsonObject>();
    JsonObjectConst changes = delta.as<JsonObjectConst>();

    for (JsonPairConst pair : changes) {
      const char* key = pair.key().c_str();
      JsonVariantConst change = pair.value();

      // Deleted property.
      if (change.is<JsonObjectConst>() && change["_deleted"].as<bool>()) {
        target.remove(key);
        continue;
      }

      // Array delta.
      if (change.is<JsonObjectConst>() && target[key].is<JsonArray>()) {
        JsonArray array = target[key].as<JsonArray>();
        JsonObjectConst arrayChanges = change.as<JsonObjectConst>();

        for (JsonPairConst item : arrayChanges) {
          const char* indexKey = item.key().c_str();

          if (strcmp(indexKey, "_size") == 0) {
            size_t newSize = item.value().as<size_t>();

            while (array.size() > newSize) {
              array.remove(array.size() - 1);
            }
            continue;
          }

          size_t index = atoi(indexKey);

          while (array.size() <= index) {
            array.add(nullptr);
          }

          array[index].set(item.value());
        }

        continue;
      }

      // Nested object delta.
      if (change.is<JsonObjectConst>() && target[key].is<JsonObject>()) {
        applyDelta(target[key], change);
        continue;
      }

      // Primitive, new property, or replacement object.
      target[key].set(change);
    }
  }
};
