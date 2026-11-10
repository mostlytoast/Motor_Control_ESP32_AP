#pragma once
#include "../../shared/utils/AppState.h"
#include "../track/map.h"

void getStateFromJson(JsonDocument& doc);

void getJsonFromState(JsonDocument& doc);

void getCondensedJsonFromStateTransmitter(JsonDocument& doc);
void getStateFromCondensedJson(JsonDocument& doc);

void getCondensedJsonFromState(JsonDocument& doc);