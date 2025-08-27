#include <driver/i2s.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include "BluetoothA2DPSource.h"
#include "esp_task_wdt.h"

const char* ssid = " ";
const char* password = " ";
const char* fileURL  = "http://192.168.1.x:x/response.wav"; //use your local server ip and the open port
const char* filePath = "/response.wav";
const char* btDeviceName = ""; //use the name of your bt device

#define I2S_WS 25
#define I2S_SD 34
#define I2S_SCK 26
#define SAMPLE_RATE 16000
#define TARGET_SIZE 524288
#define BUFFER_SIZE 1024

#define REC_LED 27
#define BLT_LED 14

int32_t i2sBuffer[BUFFER_SIZE];
BluetoothA2DPSource a2dp_source;
File audioFile;
bool playbackDone = false;

bool downloadFile(const char* url, const char* path) {
    HTTPClient http;
    http.setTimeout(10000);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        WiFiClient* stream = http.getStreamPtr();
        File file = SPIFFS.open(path, FILE_WRITE);
        if (!file) { http.end(); return false; }
        uint8_t buff[1024];
        int len = http.getSize(), total = 0;
        while (http.connected() && (len > 0 || len == -1)) {
            size_t avail = stream->available();
            if (avail) {
                int c = stream->readBytes(buff, min(avail, sizeof(buff)));
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
    if (playbackDone) return 0;
    if (!audioFile) {
        audioFile = SPIFFS.open(filePath, "r");
        if (!audioFile) { Serial.println("Error opening audio file"); playbackDone = true; return 0; }
        audioFile.seek(44, SeekSet);
    }
    int bytesRead = audioFile.read(data, len);
    if (bytesRead <= 0) { Serial.println("Playback finished!"); playbackDone = true; audioFile.close(); return 0; }
    return bytesRead;
}

void setup() {
    pinMode(REC_LED, OUTPUT);
    pinMode(BLT_LED, OUTPUT);
  
    Serial.begin(115200);
    delay(1000);
    esp_task_wdt_deinit();

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" Connected!");

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed, formatting...");
        if (!SPIFFS.format() || !SPIFFS.begin(true)) { Serial.println("SPIFFS not available"); return; }
    }

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false
    };
    i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = I2S_SD };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    SPIFFS.remove("/audio.wav");
    File file = SPIFFS.open("/audio.wav", FILE_WRITE);
    uint8_t wavHeader[44] = {0};
    file.write(wavHeader, 44);

    size_t totalBytes = 0;
    Serial.println("Recording audio...");
    
    digitalWrite(REC_LED, HIGH);
    
    while (totalBytes < TARGET_SIZE) {
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_0, (char*)i2sBuffer, sizeof(i2sBuffer), &bytesRead, portMAX_DELAY);
        int samplesRead = bytesRead / sizeof(int32_t);
        for (int i = 0; i < samplesRead; i++) {
            int16_t sample16 = (int16_t)(i2sBuffer[i] >> 14);
            file.write((uint8_t*)&sample16, sizeof(sample16));
            totalBytes += sizeof(sample16);
            if (totalBytes >= TARGET_SIZE) break;
        }
    }

    file.seek(0);
    uint32_t dataSize = totalBytes, fileSize = dataSize + 36, byteRate = SAMPLE_RATE * 2;
    uint8_t header[44] = {
        'R','I','F','F',
        (uint8_t)(fileSize & 0xFF), (uint8_t)((fileSize >> 8) & 0xFF),
        (uint8_t)((fileSize >> 16) & 0xFF), (uint8_t)((fileSize >> 24) & 0xFF),
        'W','A','V','E','f','m','t',' ', 16,0,0,0, 1,0, 1,0,
        (uint8_t)(SAMPLE_RATE & 0xFF), (uint8_t)((SAMPLE_RATE >> 8) & 0xFF),
        (uint8_t)((SAMPLE_RATE >> 16) & 0xFF), (uint8_t)((SAMPLE_RATE >> 24) & 0xFF),
        (uint8_t)(byteRate & 0xFF), (uint8_t)((byteRate >> 8) & 0xFF),
        (uint8_t)((byteRate >> 16) & 0xFF), (uint8_t)((byteRate >> 24) & 0xFF),
        2,0, 16,0,
        'd','a','t','a',
        (uint8_t)(dataSize & 0xFF), (uint8_t)((dataSize >> 8) & 0xFF),
        (uint8_t)((dataSize >> 16) & 0xFF), (uint8_t)((dataSize >> 24) & 0xFF)
    };
    file.write(header, 44);
    file.close();
    Serial.println("Recording complete");

    digitalWrite(REC_LED, LOW);

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://192.168.1.x:x/upload");
        http.addHeader("Content-Type", "audio/wav");
        File wavFile = SPIFFS.open("/audio.wav", FILE_READ);
        int code = http.sendRequest("POST", &wavFile, wavFile.size());
        wavFile.close();
        if (code > 0) Serial.printf("File uploaded, server response: %d\n", code);
        else Serial.printf("Upload error: %s\n", http.errorToString(code).c_str());
        http.end();
    }

    if (downloadFile(fileURL, filePath)) Serial.println("Download completed successfully");
    else { Serial.println("Download failed"); return; }
    
    digitalWrite(BLT_LED, HIGH);
    Serial.println("Initializing Bluetooth...");
    a2dp_source.set_data_callback(get_sound_data);
    a2dp_source.start(btDeviceName);
}

void loop() {}