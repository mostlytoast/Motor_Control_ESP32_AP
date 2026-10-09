#include <HTTPClient.h>
#include <base64.h>
#include "SpotifyClient.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>

SpotifyClient::SpotifyClient(String clientId, String clientSecret,
                             String deviceName, String refreshToken) {
  this->clientId = clientId;
  this->clientSecret = clientSecret;
  this->refreshToken = refreshToken;
  this->deviceName = deviceName;
  client.setInsecure();
}

void SpotifyClient::FetchToken() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi in FetchToken");
    return;
  }

  HTTPClient http;
  String body = "grant_type=refresh_token&refresh_token=" + refreshToken;
  String authorizationRaw = clientId + ":" + clientSecret;
  String authorization = base64::encode(authorizationRaw);

  http.begin(client, "https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", "Basic " + authorization);

  int httpCode = http.POST(body);
  Serial.print("FetchToken HTTP: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    String returnedPayload = http.getString();

    if (httpCode == 200) {
      accessToken = ParseJson("access_token", returnedPayload);
      Serial.println("Got new access token");
    } else {
      Serial.println("Failed to get new access token");
      Serial.println(returnedPayload);
    }
  } else {
    Serial.println("Failed to connect to Spotify token endpoint");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

int SpotifyClient::TransferPlayback() {
  Serial.println("TransferPlayback()");

  if (deviceId.length() == 0) {
    Serial.println("No device ID, getting devices first");
    GetDevices();
  }

  if (deviceId.length() == 0) {
    Serial.println("No device available");
    return -1;
  }

  String body = "{\"device_ids\":[\"" + deviceId + "\"],\"play\":false}";
  String url = "https://api.spotify.com/v1/me/player";

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int result = http.PUT(body);

  Serial.print("Transfer HTTP: ");
  Serial.println(result);

  http.end();
  return result;
}

int SpotifyClient::SetVolume(int volumePercent) {
  if (volumePercent < 0) volumePercent = 0;
  if (volumePercent > 100) volumePercent = 100;

  if (deviceId.length() == 0) {
    GetDevices();
  }

  String url = "https://api.spotify.com/v1/me/player/volume?volume_percent=" +
               String(volumePercent) + "&device_id=" + deviceId;

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(client, url);
  http.addHeader("Authorization", "Bearer " + accessToken);

  int result = http.PUT("");

  Serial.print("Volume HTTP: ");
  Serial.println(result);

  http.end();
  return result;
}

int SpotifyClient::Play(String spotifyUri, int offset) {
  Serial.println("Play()");

  if (accessToken.length() == 0) {
    Serial.println("No access token, fetching token first");
    FetchToken();
  }

  if (deviceId.length() == 0) {
    Serial.println("No device ID, getting devices first");
    GetDevices();
  }

  if (deviceId.length() == 0) {
    Serial.println("No Spotify device available. Turn on Echo Dot / open Spotify first.");
    return -99;
  }

  String body;

  if (spotifyUri.startsWith("spotify:track:")) {
    body = "{\"uris\":[\"" + spotifyUri + "\"]}";
  } else {
    body = "{\"context_uri\":\"" + spotifyUri + "\"";

    if (offset >= 0) {
      body += ",\"offset\":{\"position\":" + String(offset) + "}";
    }

    body += "}";
  }

  String url = "https://api.spotify.com/v1/me/player/play?device_id=" + deviceId;

  Serial.println("Spotify play body:");
  Serial.println(body);

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int result = http.PUT(body);

  Serial.print("Spotify Play HTTP: ");
  Serial.println(result);

  if (result == 204) {
    Serial.println("OK");
  } else {
    Serial.println();
  }

  if (result > 0 && result != 204) {
    String response = http.getString();
    if (response.length() > 0) Serial.println(response);
  }

  http.end();
  return result;
}

int SpotifyClient::Shuffle() {
  Serial.println("Shuffle()");

  String url =
    "https://api.spotify.com/v1/me/player/shuffle?state=true&device_id=" +
    deviceId;

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + accessToken);
  http.addHeader("Content-Length", "0");

  int result = http.PUT("");

  Serial.print("Shuffle HTTP: ");
  Serial.println(result);

  http.end();
  return result;
}

int SpotifyClient::GetShuffleState() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("GetShuffleState: WiFi not connected");
    return -1;
  }

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(client, "https://api.spotify.com/v1/me/player");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int code = http.GET();

  Serial.print("GET /me/player HTTP = ");
  Serial.println(code);

  // Refresh an expired access token and retry once.
  if (code == 401) {
    http.end();

    Serial.println("Token expired, refreshing...");

    Serial.println("Token expired, refreshing...");
    FetchToken();

    HTTPClient retry;
    retry.setTimeout(8000);
    retry.begin(client, "https://api.spotify.com/v1/me/player");
    retry.addHeader("Authorization", "Bearer " + accessToken);

    code = retry.GET();

    Serial.print("Retry HTTP = ");
    Serial.println(code);

    if (code != 200) {
      String errorBody = retry.getString();

      Serial.println("----- ERROR BODY -----");
      Serial.println(errorBody);
      Serial.println("----------------------");

      retry.end();
      return -1;
    }

    String payload = retry.getString();
    retry.end();

    Serial.println("----- PLAYER JSON -----");
    Serial.println(payload);
    Serial.println("-----------------------");

    int keyPos = payload.indexOf("\"shuffle_state\"");

    if (keyPos < 0) {
      Serial.println("shuffle_state key not found");
      return -1;
    }

    int colonPos = payload.indexOf(':', keyPos);

    if (colonPos < 0) {
      Serial.println("shuffle_state colon not found");
      return -1;
    }

    int valuePos = colonPos + 1;

    while (valuePos < payload.length() &&
           (payload[valuePos] == ' '  ||
            payload[valuePos] == '\r' ||
            payload[valuePos] == '\n' ||
            payload[valuePos] == '\t')) {
      valuePos++;
    }

    if (payload.startsWith("true", valuePos)) {
      Serial.println("Shuffle = ON");
      return 1;
    }

    if (payload.startsWith("false", valuePos)) {
      Serial.println("Shuffle = OFF");
      return 0;
    }

    Serial.println("Invalid shuffle_state value");
    return -1;
  }

  if (code != 200) {
    String errorBody = http.getString();

    Serial.println("----- ERROR BODY -----");
    Serial.println(errorBody);
    Serial.println("----------------------");

    http.end();
    return -1;
  }

  String payload = http.getString();
  http.end();

  Serial.println("----- PLAYER JSON -----");
  Serial.println(payload);
  Serial.println("-----------------------");

  int keyPos = payload.indexOf("\"shuffle_state\"");

  if (keyPos < 0) {
    Serial.println("shuffle_state key not found");
    return -1;
  }

  int colonPos = payload.indexOf(':', keyPos);

  if (colonPos < 0) {
    Serial.println("shuffle_state colon not found");
    return -1;
  }

  int valuePos = colonPos + 1;

  while (valuePos < payload.length() &&
         (payload[valuePos] == ' '  ||
          payload[valuePos] == '\r' ||
          payload[valuePos] == '\n' ||
          payload[valuePos] == '\t')) {
    valuePos++;
  }

  if (payload.startsWith("true", valuePos)) {
    Serial.println("Shuffle = ON");
    return 1;
  }

  if (payload.startsWith("false", valuePos)) {
    Serial.println("Shuffle = OFF");
    return 0;
  }

  Serial.println("Invalid shuffle_state value");
  return -1;
}

int SpotifyClient::SetShuffle(bool on) {
  Serial.print("SetShuffle: ");
  Serial.println(on ? "ON" : "OFF");

  String url =
    "https://api.spotify.com/v1/me/player/shuffle?state=" +
    String(on ? "true" : "false") +
    "&device_id=" +
    deviceId;

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(client, url);

  http.addHeader("Authorization", "Bearer " + accessToken);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Content-Length", "0");

  int result = http.PUT("");

  Serial.print("SetShuffle HTTP: ");
  Serial.println(result);

  http.end();
  return result;
}

int SpotifyClient::Next() {
  Serial.println("Next()");

  HttpResult result =
    CallAPI("POST",
            "https://api.spotify.com/v1/me/player/next?device_id=" + deviceId,
            "");

  return result.httpCode;
}


HttpResult SpotifyClient::CallAPI(String method, String url, String body) {
  HttpResult result;
  result.httpCode = 0;

  HTTPClient http;
  http.begin(client, url);
  http.addHeader(F("Content-Type"), "application/json");
  http.addHeader(F("Authorization"), "Bearer " + accessToken);

  if (body.length() == 0) {
    http.addHeader(F("Content-Length"), String(0));
  }

  if (method == "PUT") {
    result.httpCode = http.PUT(body);
  } else if (method == "POST") {
    result.httpCode = http.POST(body);
  } else if (method == "GET") {
    result.httpCode = http.GET();
  }

  if (result.httpCode > 0) {
    Serial.print("API HTTP: ");
    Serial.println(result.httpCode);

    if (http.getSize() > 0) {
      result.payload = http.getString();
    }
  } else {
    Serial.print("Failed to connect to ");
    Serial.println(url);
  }

  http.end();
  return result;
}

void SpotifyClient::GetDevices() {
  HTTPClient http;
  http.begin(client, "https://api.spotify.com/v1/me/player/devices");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int httpCode = http.GET();

  Serial.print("GetDevices HTTP: ");
  Serial.println(httpCode);

  String payload = http.getString();
  deviceId = GetDeviceId(payload);

  Serial.println((String)"Devices payload: " + payload);
  Serial.print("Selected device ID: ");
  Serial.println(deviceId);

  http.end();
}

String SpotifyClient::GetDeviceId(String json) {
  String id = "";

  Serial.print("Looking for Spotify device: ");
  Serial.println(deviceName);

  int index = json.indexOf(deviceName);

  if (index > 0) {
    int i = index;

    for (; i > 0; i--) {
      if (json.charAt(i) == '{') break;
    }

    for (; i < json.length(); i++) {
      if (json.charAt(i) == '}') break;

      if (i + 3 < json.length() &&
          json.charAt(i) == '"' &&
          json.charAt(i + 1) == 'i' &&
          json.charAt(i + 2) == 'd' &&
          json.charAt(i + 3) == '"') {
        i += 4;
        break;
      }
    }

    for (; i < json.length(); i++) {
      if (json.charAt(i) == '"') {
        i++;
        break;
      }
    }

    for (; i < json.length(); i++) {
      if (json.charAt(i) != '"') {
        id += json.charAt(i);
      } else {
        break;
      }
    }
  } else {
    Serial.print(deviceName);
    Serial.println(" device name not found.");
  }

  deviceId = id;
  return id;
}

String SpotifyClient::ParseJson(String key, String json) {
  String retVal = "";
  int index = json.indexOf(key);

  if (index > 0) {
    bool copy = false;

    for (int i = index; i < json.length(); i++) {
      if (copy) {
        if (json.charAt(i) == '"' || json.charAt(i) == ',') break;
        retVal += json.charAt(i);
      } else if (json.charAt(i) == ':') {
        copy = true;

        if (json.charAt(i + 1) == '"') {
          i++;
        }
      }
    }
  }

  return retVal;
}
