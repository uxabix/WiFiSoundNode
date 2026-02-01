#include <Arduino.h>

#include "config.h"
#include "wifi_manager.h"
#include "http_server.h"

#include <FS.h>
#include <LittleFS.h>
#include "WAVFileReader.h"
#include "DACOutput.h"
#include "TestToneSource.h"

SampleSource *sampleSource;
DACOutput *output;


void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== Sound Node starting ===");

    wifi_init();
    http_server_init();

    Serial.println("HTTP server started");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return;
    }

    Serial.println("LittleFS mounted");

    if (!LittleFS.exists("/sample.wav")) {
        Serial.println("sample.wav not found in LittleFS");
        return;
    }

    Serial.println("Created sample source");

    // sampleSource = new WAVFileReader("/sample.wav");
    sampleSource = new TestToneSource(22050);

    Serial.println("Starting I2S Output");
    output = new DACOutput();
    output->setVolume(0.1f);
    output->start(sampleSource);
}

void loop() {
    http_server_handle();
}
