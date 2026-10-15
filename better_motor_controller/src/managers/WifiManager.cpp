#include "WifiManager.h"
// ===================================================
// WIFI AP + STA
// ===================================================
void setupWifi() {
  WiFi.mode(WIFI_AP_STA);

  WiFi.softAP(AP_SSID, AP_PASSWORD, WIFI_CHANNEL);

  delay(1000);

  // Keep ESP-NOW on the AP channel.

  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t primaryChannel;
  wifi_second_chan_t secondChannel;

  esp_wifi_get_channel(&primaryChannel, &secondChannel);

  Serial.print("Actual ESP-NOW channel: ");
  Serial.println(primaryChannel);

  Serial.println();

  Serial.println("Access Point started!");

  Serial.print("Network: ");

  Serial.println(AP_SSID);

  Serial.print("IP Address: ");

  Serial.println(WiFi.softAPIP());

  Serial.print("WiFi Channel: ");

  Serial.println(WIFI_CHANNEL);

  Serial.print("Receiver MAC: ");

  Serial.println(WiFi.macAddress());
  Serial.println("=== STARTING WIFI ===");

  WiFi.mode(WIFI_AP_STA);

  Serial.println("WiFi mode set");

  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, WIFI_CHANNEL);

  Serial.print("softAP result: ");
  Serial.println(apStarted ? "SUCCESS" : "FAILED");

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);

  Serial.println("=== WIFI COMPLETE ===");
}