#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

struct HttpResult {
  int httpCode;
  String payload;
};

class SpotifyClient {
public:
  SpotifyClient(String clientId, String clientSecret, String deviceName, String refreshToken);

  void FetchToken();
  int Play(String spotifyUri, int offset = -1);
  int Shuffle();
  int Next();
  void GetDevices();
  String deviceId;
  int TransferPlayback();
  int SetVolume(int volumePercent);
  // Returns 1=ON, 0=OFF, -1=unknown/API error.
  int GetShuffleState();
  int SetShuffle(bool on);

private:
  WiFiClientSecure client;
  String clientId;
  String clientSecret;
  String redirectUri;
  String accessToken;
  String refreshToken;
  String deviceName;

  String ParseJson(String key, String json);
  HttpResult CallAPI(String method, String url, String body);
  String GetDeviceId(String json);
};
