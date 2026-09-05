#include <WiFi.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

const char* wifi_ssid = "KhatuShyam_123";
const char* wifi_password = "connect1234";

unsigned long lastWifiTry=0;
const unsigned long wifiThreshold=5000;

void init_wifi() {
  Serial.println("Setting up Wireless");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  lastWifiTry = millis();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(millis()-lastWifiTry>wifiThreshold){
      WiFi.disconnect();
      delay(100);
      WiFi.begin();
      lastWifiTry = millis();
      Serial.println("Retrying...");
    }
    
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  delay(500);
  btStop();
  Serial.println("Bluetooth Disabled");
}


String GET_Request(const char* server) {
  HTTPClient http;
  http.begin(server);
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  return payload;
}

void wirelessSleep(bool flag){
  WiFi.setSleep(flag);
}