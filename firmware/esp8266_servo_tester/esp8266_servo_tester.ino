/*
 * Krypton Bionic Arm - ESP8266 Complete Servo Tester & Controller
 * 
 * Drop-in ESP8266 alternative to the ESP32 controller.
 * Supports:
 * - Exact same packet & serial command format as ESP32 (T:...,I:...,M:...,R:...,P:...)
 * - Simultaneous varied angle testing (e.g. "T:45,I:90,M:120,R:60,P:30" or "angles 30 60 90 120 150")
 * - Direct channel-angle commands ("0 90", "1 45", "2 120", "3 60", "4 180")
 * - Trajectory smoothing / easing engine to prevent tendon snap
 * - Built-in Wi-Fi SoftAP ("Krypton-Arm" / "cyberpunk2026")
 * - Cyberpunk Web Dashboard at http://192.168.4.1 with individual finger sliders
 * - UDP Telemetry Socket on port 8888 (works with tools/servo_test.py and vision tracker)
 * - Bench sweep, zeroing ("z"), I2C scanner ("scan"), and pulse trimming ("trim")
 * 
 * Hardware Wiring:
 * - ESP8266 D2 (GPIO 4) -> PCA9685 SDA
 * - ESP8266 D1 (GPIO 5) -> PCA9685 SCL
 * - ESP8266 3V3         -> PCA9685 VCC (logic power)
 * - ESP8266 GND         -> PCA9685 GND
 * - ESP8266 VIN / 5V    -> PCA9685 V+ (6.0V from SZBK07 buck converter)
 * 
 * Serial Monitor: 115200 baud (set Newline or Both NL & CR)
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Wire.h>

// ==========================================
// Hardware Configuration for ESP8266
// ==========================================
#ifndef D1
#define D1 5
#endif
#ifndef D2
#define D2 4
#endif

#define I2C_SDA_PIN         D2        // GPIO 4
#define I2C_SCL_PIN         D1        // GPIO 5
#define I2C_CLOCK_SPEED     400000L   // 400kHz Fast Mode
#define PCA9685_ADDR        0x40      // Default I2C address
#define PWM_FREQ            50        // 50Hz (20ms cycle for MG996R)

// PCA9685 Register Map
#define PCA9685_MODE1       0x00
#define PCA9685_MODE2       0x01
#define PCA9685_PRESCALE    0xFE
#define PCA9685_LED0_ON_L   0x06

// Servo Channels (5 MG996R Servos)
#define NUM_SERVOS          5
#define CH_THUMB            0
#define CH_INDEX            1
#define CH_MIDDLE           2
#define CH_RING             3
#define CH_PINKY            4

const char* FINGER_NAMES[NUM_SERVOS] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };

// Calibrated 12-bit PWM Ticks (0-4095 at 50Hz)
// 102 ticks ~= 500 us (0 degrees / fully open)
// 512 ticks ~= 2500 us (180 degrees / clenched fist)
uint16_t pulseMinMap[NUM_SERVOS] = { 102, 105, 102, 104, 106 };
uint16_t pulseMaxMap[NUM_SERVOS] = { 510, 512, 512, 508, 505 };

// Servo state
float currentAngles[NUM_SERVOS] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
float targetAngles[NUM_SERVOS]  = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
int activeChannel = 0; // Currently selected finger for single commands

// Motion Smoothing / Trajectory Generator
bool motionSmoothingEnabled      = true;
const float TRAJECTORY_STEP_DEG  = 3.5f;   // Max degrees moved per step
const unsigned long TRAJECTORY_INTERVAL_MS = 15; // ~66 Hz easing loop
unsigned long lastTrajectoryMs   = 0;

// Network Configuration
#define AP_SSID             "Krypton-Arm"
#define AP_PASSWORD         "cyberpunk2026"
#define HTTP_SERVER_PORT    80
#define UDP_SERVER_PORT     8888
#define SERIAL_BAUD_RATE    115200

ESP8266WebServer server(HTTP_SERVER_PORT);
WiFiUDP udpServer;
char udpPacketBuffer[128];
unsigned long lastUDPPacketTime = 0;
bool udpStreamingActive = false;
#define UDP_TIMEOUT_MS 3000

// ==========================================
// Preset Poses
// ==========================================
struct GesturePose {
    const char* name;
    float angles[5]; // Thumb, Index, Middle, Ring, Pinky
};

const GesturePose PRESET_POSES[] = {
    { "OPEN",         {   0,   0,   0,   0,   0 } },
    { "FIST",         { 180, 180, 180, 180, 180 } },
    { "POINT",        { 180,   0, 180, 180, 180 } },
    { "PEACE",        { 180,   0,   0, 180, 180 } },
    { "THUMBS_UP",    {   0, 180, 180, 180, 180 } },
    { "HORNS",        { 180,   0, 180, 180,   0 } },
    { "ROCK_ON",      {   0,   0, 180, 180,   0 } },
    { "THREE_FINGER", {   0,   0,   0, 180, 180 } },
    { "PINCH",        {  90,  90,   0,   0,   0 } },
    { "OK_SIGN",      {  80,  80,   0,   0,   0 } }
};
const int NUM_PRESET_POSES = sizeof(PRESET_POSES) / sizeof(PRESET_POSES[0]);

const GesturePose* findGesture(const String& name) {
    String upper = name;
    upper.toUpperCase();
    upper.trim();
    for (int i = 0; i < NUM_PRESET_POSES; i++) {
        if (upper == PRESET_POSES[i].name) {
            return &PRESET_POSES[i];
        }
    }
    return nullptr;
}

// ==========================================
// Low-Level PCA9685 Hardware Functions
// ==========================================
void writeRegister(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(PCA9685_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(PCA9685_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)PCA9685_ADDR, (uint8_t)1);
    return Wire.read();
}

void setPWM(uint8_t channel, uint16_t on, uint16_t off) {
    Wire.beginTransmission(PCA9685_ADDR);
    Wire.write(PCA9685_LED0_ON_L + 4 * channel);
    Wire.write(on & 0xFF);
    Wire.write(on >> 8);
    Wire.write(off & 0xFF);
    Wire.write(off >> 8);
    Wire.endTransmission();
}

void setPWMFreq(float freq) {
    float prescaleval = 25000000.0f; // 25MHz internal oscillator
    prescaleval /= 4096.0f;
    prescaleval /= freq;
    prescaleval -= 1.0f;
    uint8_t prescale = (uint8_t)(prescaleval + 0.5f);

    uint8_t oldmode = readRegister(PCA9685_MODE1);
    uint8_t newmode = (oldmode & 0x7F) | 0x10; // Sleep mode
    writeRegister(PCA9685_MODE1, newmode);
    writeRegister(PCA9685_PRESCALE, prescale);
    writeRegister(PCA9685_MODE1, oldmode);
    delay(5);
    writeRegister(PCA9685_MODE1, oldmode | 0xA1); // Auto-increment and restart
}

bool initPCA9685() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_CLOCK_SPEED);

    Wire.beginTransmission(PCA9685_ADDR);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    writeRegister(PCA9685_MODE1, 0x00);
    delay(10);
    setPWMFreq(PWM_FREQ);
    writeRegister(PCA9685_MODE1, 0xA1);
    delay(5);
    return true;
}

void writeAngleHardware(uint8_t channel, float angle) {
    if (channel >= NUM_SERVOS) return;
    angle = constrain(angle, 0.0f, 180.0f);
    uint16_t pMin = pulseMinMap[channel];
    uint16_t pMax = pulseMaxMap[channel];
    uint16_t pulse = (uint16_t)map((long)(angle * 10.0f), 0, 1800, pMin, pMax);
    setPWM(channel, 0, pulse);
}

void setServoAngle(uint8_t channel, float angle, bool immediate = false) {
    if (channel >= NUM_SERVOS) return;
    angle = constrain(angle, 0.0f, 180.0f);
    targetAngles[channel] = angle;
    if (immediate || !motionSmoothingEnabled) {
        currentAngles[channel] = angle;
        writeAngleHardware(channel, angle);
    }
}

void updateTrajectory() {
    if (!motionSmoothingEnabled) return;
    unsigned long now = millis();
    if (now - lastTrajectoryMs < TRAJECTORY_INTERVAL_MS) return;
    lastTrajectoryMs = now;

    for (int i = 0; i < NUM_SERVOS; i++) {
        float diff = targetAngles[i] - currentAngles[i];
        if (abs(diff) > 0.1f) {
            if (abs(diff) <= TRAJECTORY_STEP_DEG) {
                currentAngles[i] = targetAngles[i];
            } else {
                currentAngles[i] += (diff > 0 ? TRAJECTORY_STEP_DEG : -TRAJECTORY_STEP_DEG);
            }
            writeAngleHardware(i, currentAngles[i]);
        }
    }
}

// ==========================================
// Embedded Cyberpunk Web Dashboard (HTML5 + CSS + JS)
// ==========================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Krypton ESP8266 Control Hub</title>
  <style>
    :root {
      --bg: #090c10;
      --card: #121820;
      --border: #1f2937;
      --accent: #00f0ff;
      --accent-glow: rgba(0, 240, 255, 0.25);
      --yellow: #ffb700;
      --text: #e6edf3;
      --text-dim: #8b949e;
      --success: #2ed573;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background-color: var(--bg); color: var(--text); padding: 16px; min-height: 100vh; }
    .container { max-width: 600px; margin: 0 auto; display: flex; flex-direction: column; gap: 14px; }
    header { display: flex; justify-content: space-between; align-items: center; padding: 12px 16px; background: var(--card); border: 1px solid var(--border); border-radius: 8px; }
    .brand h1 { font-size: 1.25rem; font-weight: 700; letter-spacing: 1.5px; color: var(--accent); }
    .brand p { font-size: 0.75rem; color: var(--text-dim); }
    .status-badge { display: flex; align-items: center; gap: 6px; font-size: 0.75rem; font-weight: 600; padding: 4px 10px; border-radius: 12px; background: rgba(46, 213, 115, 0.1); color: var(--success); border: 1px solid rgba(46, 213, 115, 0.3); }
    .card { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 16px; display: flex; flex-direction: column; gap: 12px; }
    .card-title { font-size: 0.85rem; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; color: var(--text-dim); display: flex; justify-content: space-between; }
    .slider-row { display: flex; flex-direction: column; gap: 6px; }
    .slider-header { display: flex; justify-content: space-between; font-size: 0.85rem; font-weight: 500; }
    .slider-val { font-family: monospace; color: var(--accent); font-weight: 700; }
    input[type=range] { -webkit-appearance: none; width: 100%; height: 8px; border-radius: 4px; background: #1c2430; outline: none; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 22px; height: 22px; border-radius: 50%; background: var(--accent); cursor: pointer; box-shadow: 0 0 10px var(--accent-glow); }
    .btn-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
    button { background: #182230; border: 1px solid var(--border); color: var(--text); padding: 10px 12px; border-radius: 6px; font-size: 0.8rem; font-weight: 600; cursor: pointer; }
    button:hover { background: #223044; border-color: var(--accent); color: var(--accent); }
    button.btn-accent { background: rgba(0, 240, 255, 0.1); border-color: var(--accent); color: var(--accent); }
    button.btn-yellow { background: rgba(255, 183, 0, 0.1); border-color: var(--yellow); color: var(--yellow); }
    .info-row { display: flex; justify-content: space-between; font-size: 0.75rem; color: var(--text-dim); border-top: 1px solid var(--border); padding-top: 6px; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="brand">
        <h1>KRYPTON // 8266</h1>
        <p>Bionic Arm Servo Controller</p>
      </div>
      <div class="status-badge">ONLINE (HTTP)</div>
    </header>

    <div class="card">
      <div class="card-title">
        <span>Varied Angle Sliders</span>
        <button onclick="sendPose('OPEN')" style="padding: 2px 8px; font-size: 0.7rem;">RESET (0&deg;)</button>
      </div>

      <div class="slider-row">
        <div class="slider-header"><span>Thumb (CH 0)</span><span class="slider-val" id="val0">0&deg;</span></div>
        <input type="range" id="ch0" min="0" max="180" value="0" oninput="setAngle(0, this.value)">
      </div>
      <div class="slider-row">
        <div class="slider-header"><span>Index (CH 1)</span><span class="slider-val" id="val1">0&deg;</span></div>
        <input type="range" id="ch1" min="0" max="180" value="0" oninput="setAngle(1, this.value)">
      </div>
      <div class="slider-row">
        <div class="slider-header"><span>Middle (CH 2)</span><span class="slider-val" id="val2">0&deg;</span></div>
        <input type="range" id="ch2" min="0" max="180" value="0" oninput="setAngle(2, this.value)">
      </div>
      <div class="slider-row">
        <div class="slider-header"><span>Ring (CH 3)</span><span class="slider-val" id="val3">0&deg;</span></div>
        <input type="range" id="ch3" min="0" max="180" value="0" oninput="setAngle(3, this.value)">
      </div>
      <div class="slider-row">
        <div class="slider-header"><span>Pinky (CH 4)</span><span class="slider-val" id="val4">0&deg;</span></div>
        <input type="range" id="ch4" min="0" max="180" value="0" oninput="setAngle(4, this.value)">
      </div>
    </div>

    <div class="card">
      <div class="card-title">Preset Gestures</div>
      <div class="btn-grid">
        <button class="btn-accent" onclick="sendPose('OPEN')">OPEN HAND</button>
        <button class="btn-accent" onclick="sendPose('FIST')">FIST</button>
        <button class="btn-accent" onclick="sendPose('POINT')">POINT</button>
        <button class="btn-yellow" onclick="sendPose('PEACE')">PEACE (V)</button>
        <button class="btn-yellow" onclick="sendPose('THUMBS_UP')">THUMBS UP</button>
        <button class="btn-yellow" onclick="sendPose('HORNS')">HORNS</button>
        <button class="btn-accent" onclick="sendPose('ROCK_ON')">ROCK ON</button>
        <button class="btn-yellow" onclick="sendPose('THREE_FINGER')">THREE</button>
        <button class="btn-accent" onclick="sendPose('PINCH')">PINCH</button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">Controller Status</div>
      <div class="info-row"><span>Driver</span><span>PCA9685 @ 0x40 (50Hz)</span></div>
      <div class="info-row"><span>I2C Pins</span><span>SDA=D2 (GPIO4), SCL=D1 (GPIO5)</span></div>
      <div class="info-row"><span>Motion Trajectory</span><span>Software Easing (~66Hz)</span></div>
      <div class="info-row"><span>UDP Telemetry</span><span>Port 8888 Active</span></div>
    </div>
  </div>

  <script>
    let sendThrottle = null;
    function setAngle(ch, val) {
      document.getElementById('val' + ch).innerText = val + '°';
      if (sendThrottle) clearTimeout(sendThrottle);
      sendThrottle = setTimeout(() => {
        fetch('/api/servos', {
          method: 'POST',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({channel: ch, angle: parseFloat(val)})
        }).catch(()=>{});
      }, 25);
    }
    function sendPose(name) {
      fetch('/api/pose', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({name: name})
      }).then(() => pollStatus()).catch(()=>{});
    }
    function pollStatus() {
      fetch('/api/status').then(r => r.json()).then(d => {
        if (d.angles) {
          for (let i = 0; i < 5; i++) {
            document.getElementById('ch' + i).value = d.angles[i];
            document.getElementById('val' + i).innerText = Math.round(d.angles[i]) + '°';
          }
        }
      }).catch(()=>{});
    }
    setInterval(pollStatus, 1500);
    window.onload = pollStatus;
  </script>
</body>
</html>)rawliteral";

// ==========================================
// Web Server Handlers
// ==========================================
void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
    String json = "{\"angles\":[";
    for (int i = 0; i < NUM_SERVOS; i++) {
        json += String(currentAngles[i], 1);
        if (i < NUM_SERVOS - 1) json += ",";
    }
    json += "],\"target\":[";
    for (int i = 0; i < NUM_SERVOS; i++) {
        json += String(targetAngles[i], 1);
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
    int chIdx = body.indexOf("\"channel\":");
    int angIdx = body.indexOf("\"angle\":");
    if (chIdx != -1 && angIdx != -1) {
        int channel = body.substring(chIdx + 10).toInt();
        float angle = body.substring(angIdx + 8).toFloat();
        setServoAngle(channel, angle);
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
                setServoAngle(i, pose->angles[i]);
            }
            server.send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
    }
    server.send(404, "application/json", "{\"error\":\"unknown_pose\"}");
}

// ==========================================
// Packet Parser (T:...,I:...,M:...,R:...,P:...)
// ==========================================
// Parses varied angles from a single line, exactly like the ESP32 packet receiver
bool parseAnglePacket(const String& line) {
    bool matched = false;
    int start = 0;
    while (start < line.length()) {
        int comma = line.indexOf(',', start);
        if (comma == -1) comma = line.indexOf(' ', start);
        if (comma == -1) comma = line.length();
        String token = line.substring(start, comma);
        token.trim();
        token.toUpperCase();

        if (token.startsWith("T:")) { setServoAngle(CH_THUMB, token.substring(2).toFloat()); matched = true; }
        else if (token.startsWith("I:")) { setServoAngle(CH_INDEX, token.substring(2).toFloat()); matched = true; }
        else if (token.startsWith("M:")) { setServoAngle(CH_MIDDLE, token.substring(2).toFloat()); matched = true; }
        else if (token.startsWith("R:")) { setServoAngle(CH_RING, token.substring(2).toFloat()); matched = true; }
        else if (token.startsWith("P:")) { setServoAngle(CH_PINKY, token.substring(2).toFloat()); matched = true; }

        start = comma + 1;
        while (start < line.length() && (line[start] == ' ' || line[start] == ',')) start++;
    }
    return matched;
}

// ==========================================
// UDP Telemetry Listener (From Python / tools)
// ==========================================
void handleUDPTelemetry() {
    int packetSize = udpServer.parsePacket();
    if (packetSize) {
        int len = udpServer.read(udpPacketBuffer, sizeof(udpPacketBuffer) - 1);
        if (len > 0) {
            udpPacketBuffer[len] = '\0';
            String msg = String(udpPacketBuffer);
            lastUDPPacketTime = millis();
            udpStreamingActive = true;
            parseAnglePacket(msg);
        }
    } else if (udpStreamingActive && (millis() - lastUDPPacketTime > UDP_TIMEOUT_MS)) {
        udpStreamingActive = false;
        Serial.println("[WATCHDOG] UDP telemetry stream idle > 3s. Holding position.");
    }
}

// ==========================================
// Serial Diagnostic / Bench Routines
// ==========================================
void zeroAllServos() {
    Serial.println("\n[*] ZEROING ALL 5 SERVOS TO 0 DEGREES (OPEN HAND)...");
    for (int i = 0; i < NUM_SERVOS; i++) {
        setServoAngle(i, 0.0f, true); // Immediate write
        delay(60);
        yield();
    }
    Serial.println("[*] Done. All servos held at 0 deg.");
    Serial.println("[!] Safe to mount yellow winch spools now.");
}

void sweepChannel(int ch, int delayMs = 15) {
    if (ch < 0 || ch >= NUM_SERVOS) return;
    Serial.printf("\n[*] Sweeping %s (CH %d): 0 -> 180 -> 0 deg...\n", FINGER_NAMES[ch], ch);

    for (int a = 0; a <= 180; a += 2) {
        writeAngleHardware(ch, a);
        currentAngles[ch] = a;
        targetAngles[ch] = a;
        delay(delayMs);
        yield();
    }
    delay(200);
    yield();
    for (int a = 180; a >= 0; a -= 2) {
        writeAngleHardware(ch, a);
        currentAngles[ch] = a;
        targetAngles[ch] = a;
        delay(delayMs);
        yield();
    }
    Serial.printf("[*] %s sweep complete.\n", FINGER_NAMES[ch]);
}

void sweepAllSimultaneous(int delayMs = 20) {
    Serial.println("\n[*] STRESS TEST: Sweeping all 5 fingers simultaneously (0 -> 180 -> 0)...");
    Serial.println("[*] Watch multimeter: SZBK07 buck output should stay rock-solid at 6.00V.");

    for (int a = 0; a <= 180; a += 2) {
        for (int ch = 0; ch < NUM_SERVOS; ch++) {
            writeAngleHardware(ch, a);
            currentAngles[ch] = a;
            targetAngles[ch] = a;
        }
        delay(delayMs);
        yield();
    }
    delay(400);
    yield();
    for (int a = 180; a >= 0; a -= 2) {
        for (int ch = 0; ch < NUM_SERVOS; ch++) {
            writeAngleHardware(ch, a);
            currentAngles[ch] = a;
            targetAngles[ch] = a;
        }
        delay(delayMs);
        yield();
    }
    Serial.println("[*] Simultaneous sweep complete.");
}

void printStatus() {
    Serial.println("\n=======================================================");
    Serial.println("        KRYPTON ESP8266 SERVO TESTER - STATUS          ");
    Serial.println("=======================================================");
    Serial.printf("Active Channel: %d (%s) | Smoothing: %s\n",
                  activeChannel, FINGER_NAMES[activeChannel],
                  motionSmoothingEnabled ? "ON (~66Hz)" : "OFF");
    Serial.println("Current Positions & Pulse Limits:");
    Serial.println(" CH | Finger  | Target Angle | Current Angle | Min Ticks | Max Ticks");
    Serial.println("----+---------+--------------+---------------+-----------+-----------");
    for (int i = 0; i < NUM_SERVOS; i++) {
        Serial.printf("  %d | %-7s |   %5.1f deg  |   %5.1f deg   |    %4d   |    %4d   %s\n",
                      i, FINGER_NAMES[i], targetAngles[i], currentAngles[i],
                      pulseMinMap[i], pulseMaxMap[i],
                      (i == activeChannel) ? "<-- [ACTIVE]" : "");
    }
    Serial.println("=======================================================");
}

void printMenu() {
    Serial.println("\n-------------------------------------------------------");
    Serial.println("                 AVAILABLE COMMANDS                    ");
    Serial.println("-------------------------------------------------------");
    Serial.println("  1. VARIED ANGLES (SIMULTANEOUS):");
    Serial.println("     T:45,I:90,M:120,R:60,P:30  : Set different angles on all fingers");
    Serial.println("     angles 30 60 90 120 150    : Shorthand list of 5 varied angles");
    Serial.println("     30 60 90 120 150           : Direct 5-angle numbers");
    Serial.println("  2. INDIVIDUAL CHANNELS:");
    Serial.println("     0 90                       : Set Channel 0 (Thumb) to 90 deg");
    Serial.println("     1 45                       : Set Channel 1 (Index) to 45 deg");
    Serial.println("     0 - 4                      : Select active finger channel");
    Serial.println("     <angle>                    : Move active finger to angle (0-180)");
    Serial.println("     all <angle>                : Move all fingers to same angle");
    Serial.println("  3. POSES & BENCH TESTING:");
    Serial.println("     POSE:<NAME> or <name>      : OPEN, FIST, POINT, PEACE, ROCK_ON, etc.");
    Serial.println("     s                          : Sweep active finger (0 -> 180 -> 0)");
    Serial.println("     w                          : Stress sweep all 5 fingers together");
    Serial.println("     z                          : ZERO ALL fingers to 0 deg (for pulleys)");
    Serial.println("     smooth                     : Toggle trajectory easing ON/OFF");
    Serial.println("     trim <min> <max>           : Pulse trim active channel (e.g. trim 104 510)");
    Serial.println("     scan                       : Run I2C bus scanner");
    Serial.println("     status                     : Show live angles table");
    Serial.println("     ? or help                  : Show this cheat sheet");
    Serial.println("-------------------------------------------------------");
}

void scanI2CBus() {
    Serial.println("\n[I2C] Scanning bus (SDA=D2/GPIO4, SCL=D1/GPIO5)...");
    byte count = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("  -> Device found at address 0x");
            if (addr < 16) Serial.print("0");
            Serial.print(addr, HEX);
            if (addr == 0x40) Serial.print(" (PCA9685 PWM Driver - MATCH!)");
            Serial.println();
            count++;
        }
        yield();
    }
    if (count == 0) {
        Serial.println("  [ERROR] No I2C devices detected! Check D2/D1 wiring and 3V3 power.");
    } else {
        Serial.printf("  Scan finished. %d device(s) found.\n", count);
    }
}

// ==========================================
// Complete Serial Command Parser
// ==========================================
void processCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    // Check 1: ESP32-style Key-Value packet: "T:45,I:90,M:120,R:60,P:30"
    if (cmd.indexOf(':') != -1 && (cmd.startsWith("T:") || cmd.startsWith("I:") || cmd.startsWith("M:") ||
                                   cmd.startsWith("R:") || cmd.startsWith("P:") || cmd.startsWith("POSE:"))) {
        if (cmd.startsWith("POSE:")) {
            String poseName = cmd.substring(5);
            const GesturePose* p = findGesture(poseName);
            if (p) {
                for (int i = 0; i < NUM_SERVOS; i++) setServoAngle(i, p->angles[i]);
                Serial.printf("[*] Pose set to [%s]: T:%.0f I:%.0f M:%.0f R:%.0f P:%.0f\n",
                              p->name, p->angles[0], p->angles[1], p->angles[2], p->angles[3], p->angles[4]);
            } else {
                Serial.println("[ERROR] Unknown pose. Options: OPEN, FIST, POINT, PEACE, THUMBS_UP, ROCK_ON, PINCH");
            }
        } else {
            parseAnglePacket(cmd);
            Serial.printf("[*] Varied angles updated: T:%.1f I:%.1f M:%.1f R:%.1f P:%.1f\n",
                          targetAngles[0], targetAngles[1], targetAngles[2], targetAngles[3], targetAngles[4]);
        }
        return;
    }

    // Check 2: Standalone Pose name (e.g. "fist", "open", "peace", "point", "rock_on")
    const GesturePose* gp = findGesture(cmd);
    if (gp) {
        for (int i = 0; i < NUM_SERVOS; i++) setServoAngle(i, gp->angles[i]);
        Serial.printf("[*] Pose [%s] applied: T:%.0f I:%.0f M:%.0f R:%.0f P:%.0f\n",
                      gp->name, gp->angles[0], gp->angles[1], gp->angles[2], gp->angles[3], gp->angles[4]);
        return;
    }

    // Check 3: Multi-angle list: "angles 30 60 90 120 150" or "30 60 90 120 150"
    String numStr = cmd;
    if (numStr.startsWith("angles ") || numStr.startsWith("ANGLES ")) {
        numStr = numStr.substring(7);
        numStr.trim();
    }
    // Count tokens
    float parsedVals[5];
    int tokenCount = 0;
    int idx = 0;
    while (idx < numStr.length() && tokenCount < 5) {
        int nextSpace = numStr.indexOf(' ', idx);
        int nextComma = numStr.indexOf(',', idx);
        int sep = -1;
        if (nextSpace != -1 && nextComma != -1) sep = min(nextSpace, nextComma);
        else if (nextSpace != -1) sep = nextSpace;
        else sep = nextComma;
        if (sep == -1) sep = numStr.length();

        String tok = numStr.substring(idx, sep);
        tok.trim();
        if (tok.length() > 0 && (isDigit(tok[0]) || (tok.length() > 1 && tok[0] == '-' && isDigit(tok[1])))) {
            parsedVals[tokenCount++] = tok.toFloat();
        }
        idx = sep + 1;
        while (idx < numStr.length() && (numStr[idx] == ' ' || numStr[idx] == ',')) idx++;
    }

    if (tokenCount == 5) {
        for (int i = 0; i < 5; i++) {
            setServoAngle(i, parsedVals[i]);
        }
        Serial.printf("[*] Varied 5-finger angles applied: T:%.1f I:%.1f M:%.1f R:%.1f P:%.1f\n",
                      targetAngles[0], targetAngles[1], targetAngles[2], targetAngles[3], targetAngles[4]);
        return;
    } else if (tokenCount == 2 && (cmd[0] >= '0' && cmd[0] <= '4')) {
        // Direct channel + angle command: "0 90" or "1 45"
        int ch = (int)parsedVals[0];
        float ang = parsedVals[1];
        if (ch >= 0 && ch < NUM_SERVOS) {
            activeChannel = ch;
            setServoAngle(ch, ang);
            Serial.printf("[*] %s (CH %d) commanded to %.1f deg\n", FINGER_NAMES[ch], ch, ang);
            return;
        }
    }

    // Check 4: Diagnostic & Helper Commands
    if (cmd == "?" || cmd.equalsIgnoreCase("help")) {
        printMenu();
    } else if (cmd.equalsIgnoreCase("status")) {
        printStatus();
    } else if (cmd.equalsIgnoreCase("scan")) {
        scanI2CBus();
    } else if (cmd.equalsIgnoreCase("z") || cmd.equalsIgnoreCase("zero")) {
        zeroAllServos();
    } else if (cmd.equalsIgnoreCase("s") || cmd.equalsIgnoreCase("sweep")) {
        sweepChannel(activeChannel);
    } else if (cmd.equalsIgnoreCase("w") || cmd.equalsIgnoreCase("stress") || cmd.equalsIgnoreCase("sweepall")) {
        sweepAllSimultaneous();
    } else if (cmd.equalsIgnoreCase("smooth")) {
        motionSmoothingEnabled = !motionSmoothingEnabled;
        Serial.printf("[*] Motion Trajectory Smoothing is now: %s\n", motionSmoothingEnabled ? "ENABLED" : "DISABLED");
    } else if (cmd.length() == 1 && cmd[0] >= '0' && cmd[0] <= '4') {
        activeChannel = cmd[0] - '0';
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else if (cmd.startsWith("all ") || cmd.startsWith("ALL ")) {
        float ang = cmd.substring(4).toFloat();
        ang = constrain(ang, 0.0f, 180.0f);
        Serial.printf("[*] Moving ALL channels to %.1f deg\n", ang);
        for (int i = 0; i < NUM_SERVOS; i++) {
            setServoAngle(i, ang);
        }
    } else if (cmd.startsWith("trim ") || cmd.startsWith("TRIM ")) {
        int spaceIdx = cmd.indexOf(' ', 5);
        if (spaceIdx != -1) {
            uint16_t nMin = cmd.substring(5, spaceIdx).toInt();
            uint16_t nMax = cmd.substring(spaceIdx + 1).toInt();
            if (nMin > 50 && nMin < nMax && nMax < 600) {
                pulseMinMap[activeChannel] = nMin;
                pulseMaxMap[activeChannel] = nMax;
                Serial.printf("[*] %s (CH %d) pulse trim updated: Min=%d, Max=%d\n",
                              FINGER_NAMES[activeChannel], activeChannel, nMin, nMax);
                writeAngleHardware(activeChannel, currentAngles[activeChannel]);
            } else {
                Serial.println("[ERROR] Invalid pulse range. Typical values: Min 90-120, Max 490-530.");
            }
        } else {
            Serial.println("[ERROR] Syntax: trim <min> <max> (e.g. 'trim 104 508')");
        }
    } else {
        // Assume single numeric angle for active channel
        float ang = cmd.toFloat();
        if (ang >= 0.0f && ang <= 180.0f) {
            setServoAngle(activeChannel, ang);
            Serial.printf("[*] %s (CH %d) target set to %.1f deg\n", FINGER_NAMES[activeChannel], activeChannel, ang);
        } else {
            Serial.println("[?] Unknown command. Type 'help' or '?' for available syntax.");
        }
    }
}

// ==========================================
// Setup & Loop
// ==========================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    Serial.println();
    Serial.println("=======================================================");
    Serial.println("       KRYPTON BIONIC ARM - ESP8266 CONTROLLER         ");
    Serial.println("=======================================================");

    // 1. Initialize PCA9685
    Serial.print("[INIT] Connecting to PCA9685 at 0x40 (SDA=D2, SCL=D1)... ");
    if (initPCA9685()) {
        Serial.println("SUCCESS (50Hz PWM Ready)");
        zeroAllServos();
    } else {
        Serial.println("FAILED!");
        Serial.println("[WARN] Could not detect PCA9685. Check 3V3/GND and D2/D1 wiring.");
        scanI2CBus();
    }

    // 2. Wi-Fi SoftAP Mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.print("[WIFI] Access Point Created: ");
    Serial.println(AP_SSID);
    Serial.print("[WIFI] Connect and browse to: http://");
    Serial.println(WiFi.softAPIP());

    // 3. Web Server Endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/servos", HTTP_POST, handleSetServos);
    server.on("/api/pose", HTTP_POST, handleSetPose);
    server.begin();
    Serial.println("[HTTP] Cyberpunk Web Dashboard active on port 80");

    // 4. UDP Telemetry Socket
    udpServer.begin(UDP_SERVER_PORT);
    Serial.printf("[UDP] Telemetry Receiver listening on port %d\n", UDP_SERVER_PORT);

    printMenu();
    printStatus();
}

void loop() {
    server.handleClient();
    handleUDPTelemetry();
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        processCommand(input);
    }
    updateTrajectory();
    yield();
}
