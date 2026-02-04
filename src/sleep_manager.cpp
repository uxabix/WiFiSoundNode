#include "sleep_manager.h"
#include "config.h"
#include <Arduino.h>
#include <esp_sleep.h>

RTC_DATA_ATTR int sunriseHour   = SUNRISE_HOUR;
RTC_DATA_ATTR int sunriseMinute = SUNRISE_MINUTE;
RTC_DATA_ATTR int sunsetHour    = SUNSET_HOUR;
RTC_DATA_ATTR int sunsetMinute  = SUNSET_MINUTE;

static bool is_night(int h, int m) {
    if (h < sunriseHour || h > sunsetHour) return true;
    if (h == sunriseHour && m < sunriseMinute) return true;
    if (h == sunsetHour && m > sunsetMinute) return true;
    return false;
}

SleepInfo sleep_get_info() {
    SleepInfo info{};
    struct tm t;
    if (!getLocalTime(&t)) {
        return info;
    }

    info.current_hour = t.tm_hour;
    info.current_minute = t.tm_min;

    info.sleep_from_hour = sunsetHour;
    info.sleep_from_minute = sunsetMinute;
    info.sleep_to_hour = sunriseHour;
    info.sleep_to_minute = sunriseMinute;

    info.night_now = is_night(t.tm_hour, t.tm_min);

    time_t now = mktime(&t);
    struct tm target = t;

    if (info.night_now) {
        target.tm_hour = sunriseHour;
        target.tm_min  = sunriseMinute;
        target.tm_sec  = 0;
        time_t wake = mktime(&target);
        if (wake <= now) wake += 24 * 3600;
        info.seconds_to_event = wake - now;
    } else {
        target.tm_hour = sunsetHour;
        target.tm_min  = sunsetMinute;
        target.tm_sec  = 0;
        time_t sleep = mktime(&target);
        if (sleep <= now) sleep += 24 * 3600;
        info.seconds_to_event = sleep - now;
    }

    return info;
}

bool sleep_should_sleep_now() {
    SleepInfo info = sleep_get_info();
    return info.night_now;
}

void sleep_go_until_wakeup() {
    SleepInfo info = sleep_get_info();
    if (!info.night_now) return;

    esp_sleep_enable_timer_wakeup(
        (uint64_t)info.seconds_to_event * 1000000ULL
    );
    esp_deep_sleep_start();
}
