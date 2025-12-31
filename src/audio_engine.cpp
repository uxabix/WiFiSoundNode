#include "audio_engine.h"
#include <Wire.h>
#include <Adafruit_MCP4725.h>

static Adafruit_MCP4725 dac;

void audio_init() {
    Wire.begin();
    dac.begin(0x60);
}

void audio_play_test() {
    // Simple chirping sound test
    for (int i = 0; i < 3; i++) {
        for (int v = 1000; v < 3000; v += 20) {
            dac.setVoltage(v, false);
            delayMicroseconds(300);
        }
        for (int v = 3000; v > 1000; v -= 20) {
            dac.setVoltage(v, false);
            delayMicroseconds(300);
        }
        delay(150);
    }

    dac.setVoltage(2048, false); // Set to mid-level to avoid pops
}
