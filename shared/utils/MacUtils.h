#pragma once
#include <Arduino.h>
#include <string.h>

#include <cstdint>
// TODO might move to utils?
String macToString(const uint8_t* mac);

bool macEqual(const uint8_t* a, const uint8_t* b);

bool isBroadcastMac(const uint8_t* mac);

void formatMac(const uint8_t* mac, char* buffer, size_t bufferSize);