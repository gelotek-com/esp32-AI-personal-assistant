#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include "BluetoothA2DPSource.h"

const char* ssid = " ";
const char* password = " ";
const char* fileURL = "http://192.168.1.x:8000/test.wav";
const char* filePath = "/test.wav";
const char* btDeviceName = " ";

BluetoothA2DPSource a2dp_source;
File audioFile;
bool playbackDone = false;

bool downloadFile(const char* url, const char* path) {
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
      Serial.println("Error opening file for writing");
      http.end();
      return false;
    }
    uint8_t buff[128];
    int len = http.getSize();
    int total = 0;
    while (http.connected() && (len > 0 || len == -1)) {
      size_t sizeAvailable = stream->available();
      if (sizeAvailable) {
        int c = stream->readBytes(buff, min(sizeAvailable, sizeof(buff)));
        file.write(buff, c);
        total += c;
        if (len > 0) len -= c;
        Serial.print(".");
      }
      delay(1);
    }
    Serial.printf("\nBytes received: %d\n", total);
    file.close();
    http.end();
    return true;
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
    http.end();
    return false;
  }
}

int32_t get_sound_data(uint8_t *data, int32_t len) {
  if (playbackDone) {
    memset(data, 0, len);
    return len;
  }
  if (!audioFile) {
    audioFile = SPIFFS.open(filePath, "r");
    if (!audioFile) {
      Serial.println("Error opening audio file");
      memset(data, 0, len);
      return len;
    }
    audioFile.seek(44, SeekSet);
  }
  int bytesRead = audioFile.read(data, len);
  if (bytesRead < len) {
    Serial.println("Playback finished!");
    playbackDone = true;
    audioFile.close();
    a2dp_source.end(); //stopping the esp from tryng to reconnect again
    memset(data + bytesRead, 0, len - bytesRead);
  }
  return len;
}

void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin(true)) {
    Serial.println("Error initializing SPIFFS");
    return;
  }
  Serial.println("SPIFFS initialized");
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  if (downloadFile(fileURL, filePath)) {
    Serial.println("Download completed successfully");
  } else {
    Serial.println("Download failed");
    return;
  }
  Serial.println("Initializing Bluetooth...");
  a2dp_source.set_data_callback(get_sound_data);
  a2dp_source.start(btDeviceName);
}

void loop() {}
