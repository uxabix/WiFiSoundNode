#pragma once
#include <Arduino.h>

void battery_init();
float battery_get_voltage();
float battery_get_percentage();
bool battery_check_critical();
