#include <Arduino.h>
#include <esp_wifi.h>

#include "config.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "AudioPlayer.h"

#include <FS.h>
#include <LittleFS.h>

AudioPlayer player(I2S_BCK, I2S_WS, I2S_DOUT, AMP_SD_PIN);

void setup() {
    Serial.begin(115200);
    delay(1000);
    randomSeed(micros());

    Serial.println();
    Serial.println("=== Sound Node starting ===");

    wifi_init();
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    setCpuFrequencyMhz(80);


    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return;
    }

    Serial.println("LittleFS mounted");

    http_server_init(player);
    Serial.println("HTTP server started");

    if (!LittleFS.exists("/glass1.wav")) {
        Serial.println("Test file not found in LittleFS. Use 'pio run -t uploadfs' to upload files.");
        return;
    }
}

void loop() {
    http_server_handle();
    // Small delay to prevent watchdog and reduce CPU usage
    // Do NOT use esp_light_sleep_start() - it blocks the async HTTP server
    delay(100);
}
