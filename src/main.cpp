#include <Arduino.h>

#include "config.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "AudioPlayer.h"

#include <FS.h>
#include <LittleFS.h>

AudioPlayer player(I2S_BCK, I2S_WS, I2S_DOUT);

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== Sound Node starting ===");

    wifi_init();
    http_server_init(player);

    Serial.println("HTTP server started");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return;
    }

    Serial.println("LittleFS mounted");

    if (!LittleFS.exists("/glass1.wav")) {
        Serial.println("Test file not found in LittleFS. Use 'pio run -t uploadfs' to upload files.");
        return;
    }
}

void loop() {
    http_server_handle();
}
