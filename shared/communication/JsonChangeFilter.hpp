
#pragma once

#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

class JsonChangeFilter {
 public:

  // Creates a delta containing only changes between two JSON values.
  static JsonDocument createDelta(
      JsonVariantConst previous,
      JsonVariantConst current) {

    JsonDocument delta;

    // Currently, the root must be an object.
    if (!previous.is<JsonObjectConst>() ||
        !current.is<JsonObjectConst>()) {
      return delta;
    }

    JsonObject changes = delta.to<JsonObject>();

    compareObjects(
        previous.as<JsonObjectConst>(),
        current.as<JsonObjectConst>(),
        changes
    );

    return delta;
  }

  // Recursively compares two JSON objects.
  static void compareObjects(
      JsonObjectConst previous,
      JsonObjectConst current,
      JsonObject changes) {

    // Check for new or modified properties.
    for (JsonPairConst pair : current) {

      const char* key = pair.key().c_str();

      JsonVariantConst oldValue = previous[key];
      JsonVariantConst newValue = pair.value();

      // New property: send the entire value.
      if (oldValue.isUnbound()) {
        changes[key].set(newValue);
        continue;
      }

      // Both are objects: recursively compare.
      if (oldValue.is<JsonObjectConst>() &&
          newValue.is<JsonObjectConst>()) {

        JsonObject nestedChanges = changes[key].to<JsonObject>();

        compareObjects(
            oldValue.as<JsonObjectConst>(),
            newValue.as<JsonObjectConst>(),
            nestedChanges
        );

        // Remove empty deltas.
        if (nestedChanges.size() == 0) {
          changes.remove(key);
        }

        continue;
      }

      // Both are arrays: recursively compare.
      if (oldValue.is<JsonArrayConst>() &&
          newValue.is<JsonArrayConst>()) {

        JsonObject arrayChanges = changes[key].to<JsonObject>();

        compareArrays(
            oldValue.as<JsonArrayConst>(),
            newValue.as<JsonArrayConst>(),
            arrayChanges
        );

        if (arrayChanges.size() == 0) {
          changes.remove(key);
        }

        continue;
      }

      // Different types or primitive values:
      // Replace the entire value.
      if (oldValue != newValue) {
        changes[key].set(newValue);
      }
    }

    // Check for deleted properties.
    for (JsonPairConst pair : previous) {

      const char* key = pair.key().c_str();

      if (current[key].isUnbound()) {
        changes[key]["_deleted"] = true;
      }
    }
  }

  // Recursively compares two arrays.
  static void compareArrays(
      JsonArrayConst previous,
      JsonArrayConst current,
      JsonObject changes) {

    // Compare existing and new elements.
    for (size_t i = 0; i < current.size(); i++) {

      String index = String(i);

      // New element: send the entire value.
      if (i >= previous.size()) {
        changes[index].set(current[i]);
        continue;
      }

      JsonVariantConst oldValue = previous[i];
      JsonVariantConst newValue = current[i];

      // Nested objects.
      if (oldValue.is<JsonObjectConst>() &&
          newValue.is<JsonObjectConst>()) {

        JsonObject nestedChanges =
            changes[index].to<JsonObject>();

        compareObjects(
            oldValue.as<JsonObjectConst>(),
            newValue.as<JsonObjectConst>(),
            nestedChanges
        );

        if (nestedChanges.size() == 0) {
          changes.remove(index);
        }

        continue;
      }

      // Nested arrays.
      if (oldValue.is<JsonArrayConst>() &&
          newValue.is<JsonArrayConst>()) {

        JsonObject nestedChanges =
            changes[index].to<JsonObject>();

        compareArrays(
            oldValue.as<JsonArrayConst>(),
            newValue.as<JsonArrayConst>(),
            nestedChanges
        );

        if (nestedChanges.size() == 0) {
          changes.remove(index);
        }

        continue;
      }

      // Primitive or different type.
      if (oldValue != newValue) {
        changes[index].set(newValue);
      }
    }

    // Handle shortened arrays.
    if (current.size() < previous.size()) {
      changes["_size"] = current.size();
    }
  }

  // Applies a delta to an existing JSON document.
  static void applyDelta(
      JsonVariant destination,
      JsonVariantConst delta) {

    if (!destination.is<JsonObject>() ||
        !delta.is<JsonObjectConst>()) {
      return;
    }

    JsonObject target = destination.as<JsonObject>();
    JsonObjectConst changes = delta.as<JsonObjectConst>();

    for (JsonPairConst pair : changes) {

      const char* key = pair.key().c_str();
      JsonVariantConst change = pair.value();

      // Property deletion.
      if (change.is<JsonObjectConst>() &&
          change["_deleted"].as<bool>()) {

        target.remove(key);
        continue;
      }

      // Array delta.
      if (change.is<JsonObjectConst>() &&
          target[key].is<JsonArray>()) {

        applyArrayDelta(
            target[key].as<JsonArray>(),
            change.as<JsonObjectConst>()
        );

        continue;
      }

      // Nested object delta.
      if (change.is<JsonObjectConst>() &&
          target[key].is<JsonObject>()) {

        applyDelta(target[key], change);
        continue;
      }

      // Primitive, new property, or replacement object/array.
      target[key].set(change);
    }
  }

  // Applies a delta to an existing array.
  static void applyArrayDelta(
      JsonArray target,
      JsonObjectConst changes) {

    for (JsonPairConst pair : changes) {

      const char* indexKey = pair.key().c_str();
      JsonVariantConst change = pair.value();

      // Resize array.
      if (strcmp(indexKey, "_size") == 0) {

        size_t newSize = change.as<size_t>();

        while (target.size() > newSize) {
          target.remove(target.size() - 1);
        }

        while (target.size() < newSize) {
          target.add(nullptr);
        }

        continue;
      }

      // Array index.
      size_t index = atoi(indexKey);

      // Ensure element exists.
      while (target.size() <= index) {
        target.add(nullptr);
      }

      JsonVariant element = target[index];

      // Nested object.
      if (change.is<JsonObjectConst>() &&
          element.is<JsonObject>()) {

        applyDelta(element, change);
        continue;
      }

      // Nested array.
      if (change.is<JsonObjectConst>() &&
          element.is<JsonArray>()) {

        applyArrayDelta(
            element.as<JsonArray>(),
            change.as<JsonObjectConst>()
        );

        continue;
      }

      // Primitive or replacement value.
      element.set(change);
    }
  }
};
