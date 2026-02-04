#include <Arduino.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <FS.h>
#include <LittleFS.h>

#include "config.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "audio_player.h"
#include "sleep_manager.h"
#include "battery.h"

AudioPlayer player(I2S_BCK, I2S_WS, I2S_DOUT, AMP_SD_PIN);

void setup() {
    Serial.begin(115200);
    delay(1000);
    randomSeed(micros());

    Serial.println("\n=== Sound Node starting ===");

    battery_init();

    wifi_init();
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    esp_wifi_set_max_tx_power(38); // ≈ 9.5 dBm
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    setCpuFrequencyMhz(80);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return;
    }

    Serial.println("LittleFS mounted");
    http_server_init(player);
    Serial.println("HTTP server started");

    player.setVolume(1.0f);
    if (!LittleFS.exists("/output.wav")) {
        Serial.println("Test file not found in LittleFS. Use 'pio run -t uploadfs'");
        return;
    }

    configTzTime(TIMEZONE, "pool.ntp.org", "time.nist.gov");
    Serial.println("Waiting for NTP time...");
    struct tm timeinfo;
    int tries = 0;
    while (!getLocalTime(&timeinfo) && tries < 10) {
        delay(1000);
        tries++;
    }
    if (tries == 10) {
        Serial.println("Failed to get NTP time, using RTC memory values");
    } else {
        Serial.println("Time synchronized via NTP");
    }
}

void loop() {
    static unsigned long lastCheck = 0;
    static unsigned long lastBatteryCheck = 0;
    unsigned long now = millis();

    if (now - lastCheck > 30*60*1000) { // Check sleep condition every 30 minutes
        lastCheck = now;

        if (sleep_should_sleep_now()) {
            Serial.println("Night detected → going to sleep");
            sleep_go_until_wakeup();
        }
    }

    if (now - lastBatteryCheck > 60*60*1000) { // Check battery every hour
        lastBatteryCheck = now;
        battery_check_critical(); // will sleep if battery is critical
    }

    if (!player.isPlaying() && !isStreaming) {
        delay(10 * 1000); // Delay to avoid busy loop when idle. Should save power while waiting for requests.
    }
}

