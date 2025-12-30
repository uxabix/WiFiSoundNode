#include <Arduino.h>

#include "config.h"
#include "wifi_manager.h"
#include "http_server.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== Sound Node starting ===");

    wifi_init();
    http_server_init();

    Serial.println("HTTP server started");
}

void loop() {
    http_server_handle();
}
