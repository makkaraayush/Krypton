#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "config.h"

WebServer server(HTTP_SERVER_PORT);

void handleRoot() {
    server.send(200, "text/plain", "Krypton WebServer Online");
}

void handleStatus() {
    server.send(200, "application/json", "{\"status\":\"ready\"}");
}

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_SPEED);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.begin();
    Serial.println("HTTP server started.");
}

void loop() {
    server.handleClient();
}
