#include <Arduino.h>

#include "config.h"
#include "wifi_manager.h"
#include "http_server.h"

#include <FS.h>
#include <LittleFS.h>


void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== Sound Node starting ===");

    wifi_init();
    http_server_init();

    Serial.println("HTTP server started");

    // if (!LittleFS.begin(true)) {
    //     Serial.println("LittleFS mount failed");
    //     return;
    // }

    // Serial.println("LittleFS mounted");

    // if (!LittleFS.exists("/sample.wav")) {
    //     Serial.println("sample.wav not found in LittleFS");
    //     return;
    // }
}

void loop() {
    http_server_handle();
}
