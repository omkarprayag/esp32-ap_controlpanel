#include <WiFi.h>
#include "wifi_setup.h"

const char* ssid = "SmartHome";
const char* password = "12345678";
const char* sta_ssid = "Mingle";
const char* sta_password = "12345678123";
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void initWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password);
  WiFi.begin(sta_ssid, sta_password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
  }
}

void maintainWiFi() {
  if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
}