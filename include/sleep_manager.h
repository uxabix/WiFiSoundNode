#pragma once
#include <time.h>

struct SleepInfo {
    bool night_now;
    int current_hour;
    int current_minute;

    int sleep_from_hour;
    int sleep_from_minute;
    int sleep_to_hour;
    int sleep_to_minute;

    int seconds_to_event; // seconds until next sunrise/sunset event (positive if in sleep period, negative if in awake period)
};

void sleep_manager_init();
SleepInfo sleep_get_info();
bool sleep_should_sleep_now();
void sleep_go_until_wakeup();
