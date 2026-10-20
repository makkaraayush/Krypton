#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiUdp.h>
#include "config.h"
#include "pca9685_driver.h"
#include "gestures.h"
#include "web_dashboard.h"

// Hardware driver and server instances
PCA9685Driver pcaDriver;
WebServer server(HTTP_SERVER_PORT);
WebSocketsServer webSocket(WS_SERVER_PORT);
WiFiUDP udpServer;

char udpPacketBuffer[128];

// ==========================================
// REST API Handlers
// ==========================================
void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
    String json = "{";
    json += "\"angles\":[";
    for (int i = 0; i < NUM_SERVOS; i++) {
        json += String(pcaDriver.getCurrentAngle(i), 1);
        if (i < NUM_SERVOS - 1) json += ",";
    }
    json += "],\"target\":[";
    for (int i = 0; i < NUM_SERVOS; i++) {
        json += String(pcaDriver.getTargetAngle(i), 1);
        if (i < NUM_SERVOS - 1) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void handleSetServos() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }
    String body = server.arg("plain");
    // Parse simple JSON { "channel": 0, "angle": 90 } or { "t": 90, "i": 90... }
    int chIdx = body.indexOf("\"channel\":");
    int angIdx = body.indexOf("\"angle\":");
    if (chIdx != -1 && angIdx != -1) {
        int channel = body.substring(chIdx + 10).toInt();
        float angle = body.substring(angIdx + 8).toFloat();
        pcaDriver.setServoAngle(channel, angle);
        server.send(200, "application/json", "{\"status\":\"ok\"}");
        return;
    }
    server.send(400, "application/json", "{\"error\":\"invalid_format\"}");
}

void handleSetPose() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }
    String body = server.arg("plain");
    int nameIdx = body.indexOf("\"name\":\"");
    if (nameIdx != -1) {
        int endIdx = body.indexOf("\"", nameIdx + 8);
        String poseName = body.substring(nameIdx + 8, endIdx);
        const GesturePose* pose = findGesture(poseName);
        if (pose) {
            for (int i = 0; i < NUM_SERVOS; i++) {
                pcaDriver.setServoAngle(i, pose->angles[i]);
            }
            server.send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
    }
    server.send(404, "application/json", "{\"error\":\"unknown_pose\"}");
}

// ==========================================
// WebSocket Handler
// ==========================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        String msg = String((char*)payload);
        // Process JSON commands
        if (msg.indexOf("\"type\":\"servo\"") != -1) {
            int chIdx = msg.indexOf("\"channel\":");
            int angIdx = msg.indexOf("\"angle\":");
            if (chIdx != -1 && angIdx != -1) {
                int ch = msg.substring(chIdx + 10).toInt();
                float ang = msg.substring(angIdx + 8).toFloat();
                pcaDriver.setServoAngle(ch, ang);
            }
        } else if (msg.indexOf("\"type\":\"pose\"") != -1) {
            int nameIdx = msg.indexOf("\"name\":\"");
            if (nameIdx != -1) {
                int endIdx = msg.indexOf("\"", nameIdx + 8);
                String poseName = msg.substring(nameIdx + 8, endIdx);
                const GesturePose* pose = findGesture(poseName);
                if (pose) {
                    for (int i = 0; i < NUM_SERVOS; i++) {
                        pcaDriver.setServoAngle(i, pose->angles[i]);
                    }
                }
            }
        }
    }
}

// ==========================================
// UDP Telemetry Listener (From Vision Script)
// ==========================================
// Expected packet: "T:45,I:90,M:120,R:120,P:120"
void handleUDPTelemetry() {
    int packetSize = udpServer.parsePacket();
    if (packetSize) {
        int len = udpServer.read(udpPacketBuffer, sizeof(udpPacketBuffer) - 1);
        if (len > 0) {
            udpPacketBuffer[len] = '\0';
            String msg = String(udpPacketBuffer);
            
            float angles[5] = { -1, -1, -1, -1, -1 };
            // Parse comma-delimited key:value pairs
            int start = 0;
            while (start < msg.length()) {
                int comma = msg.indexOf(',', start);
                if (comma == -1) comma = msg.length();
                String token = msg.substring(start, comma);
                token.trim();
                if (token.startsWith("T:")) angles[0] = token.substring(2).toFloat();
                else if (token.startsWith("I:")) angles[1] = token.substring(2).toFloat();
                else if (token.startsWith("M:")) angles[2] = token.substring(2).toFloat();
                else if (token.startsWith("R:")) angles[3] = token.substring(2).toFloat();
                else if (token.startsWith("P:")) angles[4] = token.substring(2).toFloat();
                start = comma + 1;
            }

            for (int i = 0; i < NUM_SERVOS; i++) {
                if (angles[i] >= 0.0f) {
                    pcaDriver.setServoAngle(i, angles[i]);
                }
            }
        }
    }
}

// ==========================================
// Serial USB Protocol (Wired Fallback)
// ==========================================
void handleSerialCommands() {
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) return;

        if (line.startsWith("POSE:")) {
            String poseName = line.substring(5);
            const GesturePose* p = findGesture(poseName);
            if (p) {
                for (int i = 0; i < NUM_SERVOS; i++) {
                    pcaDriver.setServoAngle(i, p->angles[i]);
                }
                Serial.println("OK:POSE_SET");
            } else {
                Serial.println("ERR:UNKNOWN_POSE");
            }
        } else if (line.indexOf(':') != -1) {
            // "T:90,I:120,M:120,R:120,P:120"
            int start = 0;
            while (start < line.length()) {
                int comma = line.indexOf(',', start);
                if (comma == -1) comma = line.length();
                String token = line.substring(start, comma);
                token.trim();
                if (token.startsWith("T:")) pcaDriver.setServoAngle(CH_THUMB, token.substring(2).toFloat());
                else if (token.startsWith("I:")) pcaDriver.setServoAngle(CH_INDEX, token.substring(2).toFloat());
                else if (token.startsWith("M:")) pcaDriver.setServoAngle(CH_MIDDLE, token.substring(2).toFloat());
                else if (token.startsWith("R:")) pcaDriver.setServoAngle(CH_RING, token.substring(2).toFloat());
                else if (token.startsWith("P:")) pcaDriver.setServoAngle(CH_PINKY, token.substring(2).toFloat());
                start = comma + 1;
            }
            Serial.println("OK:ANGLES_UPDATED");
        }
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);
    Serial.println();
    Serial.println("============================================");
    Serial.println("       KRYPTON BIONIC ARM CONTROLLER        ");
    Serial.println("============================================");

    // 1. Initialize PCA9685 PWM Driver
    Serial.print("[INIT] Connecting to PCA9685 on I2C (SDA=21, SCL=22)... ");
    if (pcaDriver.begin()) {
        Serial.println("OK (0x40 @ 50Hz)");
    } else {
        Serial.println("FAILED! Check wiring and 6V power.");
    }

    // 2. Wi-Fi Configuration (SoftAP)
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);
    Serial.print("[WIFI] Access Point Created: ");
    Serial.println(AP_SSID);
    Serial.print("[WIFI] AP Gateway IP: ");
    Serial.println(WiFi.softAPIP());

    // 3. Web & WebSocket Server
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/servos", HTTP_POST, handleSetServos);
    server.on("/api/pose", HTTP_POST, handleSetPose);
    server.begin();
    Serial.println("[HTTP] Web Dashboard Server started on port 80");

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("[WS] WebSocket Server started on port 81");

    // 4. UDP Telemetry Socket
    udpServer.begin(UDP_SERVER_PORT);
    Serial.print("[UDP] Telemetry Receiver listening on port ");
    Serial.println(UDP_SERVER_PORT);

    Serial.println("[READY] System initialized. Servos homed to 0 deg.");
}

void loop() {
    server.handleClient();
    webSocket.loop();
    handleUDPTelemetry();
    handleSerialCommands();
    pcaDriver.updateTrajectory();
}
