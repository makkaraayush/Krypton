#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include "config.h"
#include "gestures.h"
#include "web_dashboard.h"

WebServer server(HTTP_SERVER_PORT);
WebSocketsServer webSocket(WS_SERVER_PORT);

void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        Serial.printf("[WS] Received: %s\n", payload);
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_SPEED);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    server.on("/", HTTP_GET, handleRoot);
    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

void loop() {
    server.handleClient();
    webSocket.loop();
}
