#include <Arduino.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <FS.h>
#include <LittleFS.h>

#include "config.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "AudioPlayer.h"
#include "battery.h"

AudioPlayer player(I2S_BCK, I2S_WS, I2S_DOUT, AMP_SD_PIN);

// RTC memory to store sunrise/sunset times
RTC_DATA_ATTR int sunriseHour = SUNRISE_HOUR;
RTC_DATA_ATTR int sunriseMinute = SUNRISE_MINUTE;
RTC_DATA_ATTR int sunsetHour  = SUNSET_HOUR;
RTC_DATA_ATTR int sunsetMinute = SUNSET_MINUTE;

void checkSunTimesAndSleep();
void goToSleepUntilNextWake();

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

void checkSunTimesAndSleep() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time, using RTC memory values");
        return;
    }

    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;

    Serial.printf("Current time: %02d:%02d\n", currentHour, currentMinute);
    Serial.printf("Sunrise: %02d:%02d, Sunset: %02d:%02d\n",
                  sunriseHour, sunriseMinute, sunsetHour, sunsetMinute);

    if ((currentHour < sunriseHour) || (currentHour > sunsetHour) ||
        (currentHour == sunriseHour && currentMinute < sunriseMinute) ||
        (currentHour == sunsetHour && currentMinute > sunsetMinute)) {

        Serial.println("Entering deep sleep until sunrise...");
        goToSleepUntilNextWake();
    }
}

void goToSleepUntilNextWake() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Cannot get time, skipping sleep");
        return;
    }

    time_t now = mktime(&timeinfo);

    struct tm nextWake = timeinfo;
    nextWake.tm_hour = sunriseHour;
    nextWake.tm_min = sunriseMinute;
    nextWake.tm_sec = 0;

    time_t wakeTime = mktime(&nextWake);
    if (wakeTime <= now) wakeTime += 24*3600; // next day

    int secondsToWake = wakeTime - now;
    Serial.printf("Deep sleep for %d seconds\n", secondsToWake);

    esp_sleep_enable_timer_wakeup((uint64_t)secondsToWake*1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    static unsigned long lastCheck = 0;
    unsigned long now = millis();

    if (now - lastCheck > 30*60*1000) { // every 30 minutes
        lastCheck = now;
        checkSunTimesAndSleep();
    }

    if (!player.isPlaying() && !isStreaming) {
        delay(2000);
    }
}
