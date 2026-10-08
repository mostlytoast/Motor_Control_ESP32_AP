#include "MacUtils.h"
// =====================================================
// MAC UTILITIES
// =====================================================

String macToString(const uint8_t* mac) {
  char buffer[18];

  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);

  return String(buffer);
}

// =====================================================
// MAC COMPARISON
// =====================================================

bool macEqual(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

// =====================================================
// DISCOVERY RESPONSE
// =====================================================
bool isBroadcastMac(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) {
      return false;
    }
  }

  return true;
}

// // =====================================================
// // FORMAT MAC
// // =====================================================

void formatMac(const uint8_t* mac, char* buffer, size_t bufferSize) {
  if (mac == NULL || buffer == NULL || bufferSize == 0) {
    return;
  }

  snprintf(buffer, bufferSize, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
}
