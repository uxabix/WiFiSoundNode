#include "wifi_manager.h"
#include "config.h"

IPAddress local_IP(LOCAL_IP); // Set a static IP address
IPAddress gateway(GATEWAY);
IPAddress subnet(SUBNET);
IPAddress dns1(DNS1);
IPAddress dns2(DNS2);

void wifi_init() {
    WiFi.disconnect(true);
    delay(800);
    WiFi.mode(WIFI_STA);
    delay(200);

    #ifdef USE_STATIC_IP
        WiFi.config(local_IP, gateway, subnet, dns1, dns2);
    #endif

    Serial.print("Connecting to WiFi...\n");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (true) {
        wl_status_t st = WiFi.status();

        if (st == WL_CONNECTED) break;

        if (st == WL_NO_SSID_AVAIL || st == WL_CONNECT_FAILED) {
            Serial.println("Retrying WiFi...\n");
            WiFi.disconnect(false);
            delay(500);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }

        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("WiFi connect timeout → restart");
            WiFi.disconnect(true);
            delay(2000);
            ESP.restart();
        }

        delay(500);
    }

    Serial.println();
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}
