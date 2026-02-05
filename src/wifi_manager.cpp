#include "wifi_manager.h"
#include "config.h"

IPAddress local_IP(LOCAL_IP); // Set a static IP address
IPAddress gateway(GATEWAY);
IPAddress subnet(SUBNET);
IPAddress dns1(DNS1);
IPAddress dns2(DNS2);

void wifi_init() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}
