#pragma once

#include "../../shared/utils/AppState.h"

void getStateFromJson(JsonDocument& doc);

void getJsonFromState(JsonDocument& doc);

void getStateFromCondensedJson(JsonDocument& doc);

void getCondensedJsonFromState(JsonDocument& doc);