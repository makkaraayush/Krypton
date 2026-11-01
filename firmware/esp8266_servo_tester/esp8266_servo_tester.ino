/*
 * Krypton Bionic Arm - Standalone ESP8266 Servo Test Suite
 * 
 * Purpose:
 * Bench testing, electrical zeroing, tendon tension checking, varied angle testing,
 * and pulse trimming for TowerPro MG996R servos via PCA9685 I2C driver (or direct GPIO).
 * Exact drop-in ESP8266 version of esp32_servo_tester.
 * 
 * Hardware:
 * - ESP8266 (NodeMCU / D1 Mini)
 *   - I2C SDA: Pin D2 (GPIO 4)
 *   - I2C SCL: Pin D1 (GPIO 5)
 *   - Optional Direct Servo: Pin D5 (GPIO 14)
 * - PCA9685 16-Channel 12-bit PWM Driver (I2C 0x40, 50Hz)
 * - SZBK07 300W 20A Buck Converter (Input: 12V Battery -> Output: 6.0V DC)
 * - 5x TowerPro MG996R Servos (Channels 0 to 4)
 * 
 * Wiring (5-wire ribbon from PCA9685 left header to ESP8266):
 * - PCA9685 V+ (6.0V)  -> ESP8266 VIN / 5V pin (feeds onboard 3.3V LDO regulator)
 * - PCA9685 GND        -> ESP8266 GND pin
 * - PCA9685 VCC (3.3V) <- ESP8266 3V3 pin (powers PCA9685 logic)
 * - PCA9685 SDA        <- ESP8266 D2 (GPIO 4)
 * - PCA9685 SCL        <- ESP8266 D1 (GPIO 5)
 * 
 * Serial Monitor:
 * Baud Rate: 115200 (set "Newline" or "Both NL & CR" in Arduino IDE)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>

// ==========================================
// Hardware Configuration
// ==========================================
#ifndef D1
#define D1 5
#endif
#ifndef D2
#define D2 4
#endif
#ifndef D5
#define D5 14
#endif

#define I2C_SDA_PIN         D2        // GPIO 4
#define I2C_SCL_PIN         D1        // GPIO 5
#define I2C_CLOCK_SPEED     400000    // 400kHz Fast Mode
#define PCA9685_ADDR        0x40      // Default address
#define PWM_FREQ            50        // 50Hz (20ms cycle for MG996R)

// PCA9685 Register Map
#define PCA9685_MODE1       0x00
#define PCA9685_MODE2       0x01
#define PCA9685_PRESCALE    0xFE
#define PCA9685_LED0_ON_L   0x06

// Servo Channels
#define NUM_SERVOS          5
const char* FINGER_NAMES[NUM_SERVOS] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };

// Calibrated 12-bit PWM Ticks (0-4095 at 50Hz)
// 102 ticks ~= 500 us (0 degrees / fully open)
// 512 ticks ~= 2500 us (180 degrees / clenched fist)
uint16_t pulseMin[NUM_SERVOS] = { 102, 105, 102, 104, 106 };
uint16_t pulseMax[NUM_SERVOS] = { 510, 512, 512, 508, 505 };

// Current angles
float currentAngles[NUM_SERVOS] = { 0, 0, 0, 0, 0 };
int activeChannel = 0; // Currently selected finger (0 to 4)

// Optional Single Servo Direct GPIO Test Pin (if testing without PCA9685)
#define DIRECT_GPIO_PIN     D5        // GPIO 14 on ESP8266
Servo directServo;
bool directModeEnabled      = false;

// ==========================================
// Low-Level PCA9685 Driver Functions
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

    // Verify presence
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
    currentAngles[channel] = angle;

    uint16_t pMin = pulseMin[channel];
    uint16_t pMax = pulseMax[channel];
    uint16_t pulse = (uint16_t)map((long)(angle * 10.0f), 0, 1800, pMin, pMax);
    setPWM(channel, 0, pulse);
}

void initDirectGPIOServo() {
    directServo.attach(DIRECT_GPIO_PIN, 500, 2500);
    directModeEnabled = true;
}

void setDirectGPIOServo(float angle) {
    angle = constrain(angle, 0.0f, 180.0f);
    directServo.write((int)angle);
}

// ==========================================
// I2C Bus Scanner
// ==========================================
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
        Serial.println("  [ERROR] No I2C devices detected!");
        Serial.println("  Checklist:");
        Serial.println("   1. Verify ESP8266 D2 (GPIO 4) goes to PCA9685 SDA.");
        Serial.println("   2. Verify ESP8266 D1 (GPIO 5) goes to PCA9685 SCL.");
        Serial.println("   3. Verify ESP8266 3V3 goes to PCA9685 VCC (logic power).");
        Serial.println("   4. Verify common GND is connected between ESP8266 and PCA9685.");
    } else {
        Serial.printf("  Scan finished. %d device(s) found.\n", count);
    }
}

// ==========================================
// Test Routines
// ==========================================
void zeroAllServos() {
    Serial.println("\n[*] ZEROING ALL 5 SERVOS TO 0 DEGREES (OPEN HAND)...");
    for (int i = 0; i < NUM_SERVOS; i++) {
        writeAngleHardware(i, 0.0f);
        delay(80);
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
        delay(delayMs);
        yield();
    }
    delay(200);
    yield();
    for (int a = 180; a >= 0; a -= 2) {
        writeAngleHardware(ch, a);
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
        }
        delay(delayMs);
        yield();
    }
    delay(400);
    yield();
    for (int a = 180; a >= 0; a -= 2) {
        for (int ch = 0; ch < NUM_SERVOS; ch++) {
            writeAngleHardware(ch, a);
        }
        delay(delayMs);
        yield();
    }
    Serial.println("[*] Simultaneous sweep complete.");
}

void applyPose(const char* name, float t, float i, float m, float r, float p) {
    Serial.printf("\n[*] Applying Pose [%s]: T:%.0f I:%.0f M:%.0f R:%.0f P:%.0f\n", name, t, i, m, r, p);
    writeAngleHardware(0, t);
    writeAngleHardware(1, i);
    writeAngleHardware(2, m);
    writeAngleHardware(3, r);
    writeAngleHardware(4, p);
}

void printStatus() {
    Serial.println("\n=======================================================");
    Serial.println("           KRYPTON SERVO TESTER - STATUS               ");
    Serial.println("=======================================================");
    Serial.printf("Active Selected Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    Serial.println("Current Positions & Pulse Limits:");
    Serial.println(" CH | Finger  | Current Angle | Pulse Min | Pulse Max");
    Serial.println("----+---------+---------------+-----------+-----------");
    for (int i = 0; i < NUM_SERVOS; i++) {
        Serial.printf("  %d | %-7s |    %5.1f deg  |    %4d   |    %4d   %s\n",
                      i, FINGER_NAMES[i], currentAngles[i], pulseMin[i], pulseMax[i],
                      (i == activeChannel) ? "<-- [ACTIVE]" : "");
    }
    Serial.println("=======================================================");
}

void printMenu() {
    Serial.println("\n-------------------------------------------------------");
    Serial.println("                 AVAILABLE COMMANDS                    ");
    Serial.println("-------------------------------------------------------");
    Serial.println("  0 - 4       : Select active finger (0:Thumb, 1:Index, 2:Mid, 3:Ring, 4:Pinky)");
    Serial.println("  <angle>     : Move active finger to angle 0-180 (e.g. '0', '90', '180')");
    Serial.println("  <ch> <ang>  : Move specific channel directly (e.g. '0 45', '1 90', '2 120')");
    Serial.println("  all <angle> : Move ALL fingers to same angle (e.g. 'all 0', 'all 90')");
    Serial.println("  T:..,I:..   : Varied angles packet (e.g. 'T:45,I:90,M:120,R:60,P:30')");
    Serial.println("  <5 angles>  : List of 5 varied angles (e.g. '30 60 90 120 150')");
    Serial.println("  s           : Sweep active finger (0 -> 180 -> 0)");
    Serial.println("  w           : Stress test: sweep ALL 5 fingers together");
    Serial.println("  z           : ZERO ALL fingers to 0 deg (for installing horns)");
    Serial.println("  p           : Cycle through preset poses (Fist, Peace, Point, etc.)");
    Serial.println("  scan        : Re-run I2C bus scanner");
    Serial.println("  status      : Print current angles and pulse trims");
    Serial.println("  trim <min> <max> : Set pulse limits for active channel (e.g. 'trim 104 508')");
    Serial.println("  gpio <angle>: Test direct ESP8266 pin D5 (without PCA9685)");
    Serial.println("  ? or help   : Show this help menu");
    Serial.println("-------------------------------------------------------");
}

// ==========================================
// Command Parser
// ==========================================
int poseIndex = 0;
void cyclePose() {
    switch (poseIndex) {
        case 0: applyPose("OPEN HAND", 0, 0, 0, 0, 0); break;
        case 1: applyPose("FULL FIST", 180, 180, 180, 180, 180); break;
        case 2: applyPose("POINT", 180, 0, 180, 180, 180); break;
        case 3: applyPose("PEACE (V)", 180, 0, 0, 180, 180); break;
        case 4: applyPose("THUMBS UP", 0, 180, 180, 180, 180); break;
        case 5: applyPose("ROCK ON", 0, 0, 180, 180, 0); break;
    }
    poseIndex = (poseIndex + 1) % 6;
}

void parseAnglePacket(const String& line) {
    int start = 0;
    while (start < line.length()) {
        int comma = line.indexOf(',', start);
        if (comma == -1) comma = line.indexOf(' ', start);
        if (comma == -1) comma = line.length();
        String token = line.substring(start, comma);
        token.trim();
        token.toUpperCase();

        if (token.startsWith("T:")) writeAngleHardware(0, token.substring(2).toFloat());
        else if (token.startsWith("I:")) writeAngleHardware(1, token.substring(2).toFloat());
        else if (token.startsWith("M:")) writeAngleHardware(2, token.substring(2).toFloat());
        else if (token.startsWith("R:")) writeAngleHardware(3, token.substring(2).toFloat());
        else if (token.startsWith("P:")) writeAngleHardware(4, token.substring(2).toFloat());

        start = comma + 1;
        while (start < line.length() && (line[start] == ' ' || line[start] == ',')) start++;
    }
}

void processCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

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
    } else if (cmd.equalsIgnoreCase("p") || cmd.equalsIgnoreCase("pose")) {
        cyclePose();
    } else if (cmd.indexOf(':') != -1 && (cmd.startsWith("T:") || cmd.startsWith("I:") || cmd.startsWith("M:") ||
                                         cmd.startsWith("R:") || cmd.startsWith("P:") || cmd.startsWith("t:") ||
                                         cmd.startsWith("i:") || cmd.startsWith("m:") || cmd.startsWith("r:") ||
                                         cmd.startsWith("p:"))) {
        parseAnglePacket(cmd);
        Serial.printf("[*] Varied angles set: T:%.0f I:%.0f M:%.0f R:%.0f P:%.0f\n",
                      currentAngles[0], currentAngles[1], currentAngles[2], currentAngles[3], currentAngles[4]);
    } else if (cmd.startsWith("all ") || cmd.startsWith("ALL ")) {
        float ang = cmd.substring(4).toFloat();
        ang = constrain(ang, 0.0f, 180.0f);
        Serial.printf("[*] Moving ALL channels to %.1f deg\n", ang);
        for (int i = 0; i < NUM_SERVOS; i++) {
            writeAngleHardware(i, ang);
        }
    } else if (cmd.startsWith("trim ") || cmd.startsWith("TRIM ")) {
        int spaceIdx = cmd.indexOf(' ', 5);
        if (spaceIdx != -1) {
            uint16_t nMin = cmd.substring(5, spaceIdx).toInt();
            uint16_t nMax = cmd.substring(spaceIdx + 1).toInt();
            if (nMin > 50 && nMin < nMax && nMax < 600) {
                pulseMin[activeChannel] = nMin;
                pulseMax[activeChannel] = nMax;
                Serial.printf("[*] %s (CH %d) pulse trim updated: Min=%d, Max=%d\n",
                              FINGER_NAMES[activeChannel], activeChannel, nMin, nMax);
                writeAngleHardware(activeChannel, currentAngles[activeChannel]);
            } else {
                Serial.println("[ERROR] Invalid pulse range. Typical values: Min 90-120, Max 490-530.");
            }
        } else {
            Serial.println("[ERROR] Syntax: trim <min> <max> (e.g. 'trim 104 508')");
        }
    } else if (cmd.startsWith("gpio ") || cmd.startsWith("GPIO ")) {
        float ang = cmd.substring(5).toFloat();
        if (!directModeEnabled) {
            initDirectGPIOServo();
            Serial.printf("[*] Initialized direct ESP8266 pin D5 (GPIO %d) PWM.\n", DIRECT_GPIO_PIN);
        }
        setDirectGPIOServo(ang);
        Serial.printf("[*] Direct pin D5 commanded to %.1f deg.\n", ang);
    } else if (cmd.length() == 1 && cmd[0] >= '0' && cmd[0] <= '4') {
        activeChannel = cmd[0] - '0';
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else {
        // Check for 5 varied angle numbers: e.g. "30 60 90 120 150" or "angles 30 60 90 120 150"
        String numStr = cmd;
        if (numStr.startsWith("angles ") || numStr.startsWith("ANGLES ")) {
            numStr = numStr.substring(7);
            numStr.trim();
        }
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
                writeAngleHardware(i, parsedVals[i]);
            }
            Serial.printf("[*] Varied 5-finger angles applied: T:%.0f I:%.0f M:%.0f R:%.0f P:%.0f\n",
                          currentAngles[0], currentAngles[1], currentAngles[2], currentAngles[3], currentAngles[4]);
        } else if (tokenCount == 2 && (cmd[0] >= '0' && cmd[0] <= '4')) {
            // Direct channel + angle command: "0 90" or "1 45"
            int ch = (int)parsedVals[0];
            float ang = parsedVals[1];
            if (ch >= 0 && ch < NUM_SERVOS) {
                activeChannel = ch;
                writeAngleHardware(ch, ang);
                Serial.printf("[*] %s (CH %d) set to %.1f deg\n", FINGER_NAMES[ch], ch, ang);
            }
        } else {
            // Assume single numeric angle for active channel
            float ang = cmd.toFloat();
            if (ang >= 0.0f && ang <= 180.0f) {
                writeAngleHardware(activeChannel, ang);
                Serial.printf("[*] %s (CH %d) set to %.1f deg\n", FINGER_NAMES[activeChannel], activeChannel, ang);
            } else {
                Serial.println("[?] Unknown command. Type 'help' or '?' for available commands.");
            }
        }
    }
}

// ==========================================
// Arduino Setup & Loop
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(800);

    Serial.println();
    Serial.println("=======================================================");
    Serial.println("        KRYPTON BIONIC ARM - ESP8266 SERVO TESTER      ");
    Serial.println("=======================================================");
    Serial.println("Designed for 12V Battery + SZBK07 20A Buck + PCA9685");

    // Initialize I2C and check PCA9685
    Serial.print("[INIT] Connecting to PCA9685 at 0x40 (SDA=D2, SCL=D1)... ");
    if (initPCA9685()) {
        Serial.println("SUCCESS (50Hz PWM Ready)");
        // Safely zero all servos on boot
        zeroAllServos();
    } else {
        Serial.println("FAILED!");
        Serial.println("[WARN] Could not communicate with PCA9685 at 0x40.");
        scanI2CBus();
    }

    printMenu();
    printStatus();
}

void loop() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        processCommand(input);
    }
    yield();
}
