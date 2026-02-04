#include "battery.h"
#include "config.h"
#include <esp_sleep.h>

void battery_init() {
    analogReadResolution(12); // 0..4095
    analogSetAttenuation(ADC_11db);
}

float battery_get_voltage() {
    int raw = analogRead(BTR_ADC_PIN);
    float adc_voltage = (raw / 4095.0f) * 3.6f; // ESP32 ADC ~0-3.6V
    float battery_voltage = adc_voltage * ((BATT_DIV_R1 + BATT_DIV_R2) / BATT_DIV_R2);
    battery_voltage *= BATT_CAL_FACTOR; // Apply calibration factor
    return battery_voltage;
}

float battery_get_percentage() {
    float voltage = battery_get_voltage();
    float percent = (voltage - BATT_MIN_VOLTAGE) / (BATT_MAX_VOLTAGE - BATT_MIN_VOLTAGE) * 100.0f;
    if (percent > 100.0f) percent = 100.0f;
    if (percent < 0.0f) percent = 0.0f;
    return percent;
}

// Returns true if device is sleeping due to critical voltage
bool battery_check_critical() {
    float voltage = battery_get_voltage();
    if (voltage < BATT_CRITICAL_VOLTAGE) {
        Serial.printf("Battery critically low: %.2fV. Entering deep sleep for %d seconds...\n",
                      voltage, BATTERY_WAKEUP_INTERVAL);

        esp_sleep_enable_timer_wakeup((uint64_t)BATTERY_WAKEUP_INTERVAL * 1000000ULL);
        esp_deep_sleep_start();
        return true; // should never return
    }
    return false;
}
