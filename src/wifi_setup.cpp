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
  unsigned long startAttemptTime = millis();

  WiFi.mode(WIFI_AP_STA);

  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password);
  Serial.println("[AP] SoftAP started");
  Serial.print("[AP] IP address: ");
  Serial.println(WiFi.softAPIP());

  WiFi.begin(sta_ssid, sta_password);
  Serial.print("[STA] Connecting to WiFi");
 
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[STA] Connected!");
    Serial.print("[STA] IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[STA] Failed to connect");
  }
}

void maintainWiFi() {
  if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
}