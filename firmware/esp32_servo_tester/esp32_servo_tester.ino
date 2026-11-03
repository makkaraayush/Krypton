/*
 * Krypton Bionic Arm - Standalone ESP32 Servo Test Suite (6-Channel)
 * 
 * Purpose:
 * Bench testing, electrical zeroing, tendon tension checking, wrist rotation testing,
 * wrist-tendon decoupling compensation, and pulse trimming for TowerPro MG996R servos.
 * 
 * Channels:
 * - CH 0: Thumb     (0 = Open, 180 = Clenched, Pulse: 102 - 540)
 * - CH 1: Index     (0 = Open, 180 = Clenched, Pulse: 102 - 550 [+10 ticks calibrated])
 * - CH 2: Middle    (0 = Open, 180 = Clenched, Pulse: 102 - 545 [+5 ticks calibrated])
 * - CH 3: Ring      (0 = Open, 180 = Clenched, Pulse: 102 - 540)
 * - CH 4: Pinky     (0 = Open, 180 = Clenched, Pulse: 102 - 540)
 * - CH 5: Wrist     (90 = Straight/Neutral, 0 = Left, 180 = Right, Pulse: 102 - 540)
 * 
 * Hardware:
 * - ESP32 Type-C CP2102 (SDA = GPIO 21, SCL = GPIO 22)
 * - PCA9685 16-Channel 12-bit PWM Driver (I2C 0x40, 50Hz)
 * - SZBK07 300W 20A Buck Converter (Input: 12V Battery -> Output: 6.0V DC)
 * - 6x TowerPro MG996R Servos
 * 
 * Serial Monitor:
 * Baud Rate: 115200 (set "Newline" or "Both NL & CR" in Arduino IDE)
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_arduino_version.h>

// ==========================================
// Hardware Configuration
// ==========================================
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define I2C_CLOCK_SPEED     400000    // 400kHz Fast Mode
#define PCA9685_ADDR        0x40      // Default address
#define PWM_FREQ            50        // 50Hz (20ms cycle for MG996R)

// PCA9685 Register Map
#define PCA9685_MODE1       0x00
#define PCA9685_MODE2       0x01
#define PCA9685_PRESCALE    0xFE
#define PCA9685_LED0_ON_L   0x06

// Servo Channels
#define NUM_SERVOS          6
const char* FINGER_NAMES[NUM_SERVOS] = { "Thumb", "Index", "Middle", "Ring", "Pinky", "Wrist" };
#define SERVO_NAMES         FINGER_NAMES

// Calibrated 12-bit PWM Ticks (0-4095 at 50Hz)
// 102 ticks ~= 500 us (0 degrees)
// 540 ticks ~= 2500-2636 us (180 degrees / full tight grip)
// Per-finger custom calibration:
//   Index  (CH 1): 550 (+10 ticks for full tight curl)
//   Middle (CH 2): 545 (+5 ticks for full tight curl)
uint16_t pulseMin[NUM_SERVOS] = { 102, 102, 102, 102, 102, 102 };
uint16_t pulseMax[NUM_SERVOS] = { 540, 550, 545, 540, 540, 540 };

// Current user commanded target angles
// Fingers default to 0 deg (open), Wrist defaults to 90 deg (straight)
float currentAngles[NUM_SERVOS] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 90.0f };
int activeChannel = 0; // Currently selected servo (0 to 5)

// ==========================================
// Wrist Rotation Tendon Decoupling Compensation
// ==========================================
// As the wrist rotates away from 90 deg, tendons routing around the rotating collar
// change path length. Rotating left (<90) tightens outer fingers (especially Pinky/Ring).
// Compensation formula:
//   delta_wrist = currentAngles[5] - 90.0 deg
//   hw_angle[i] = target_angle[i] + (wristCompCoeffs[i] * delta_wrist)
// When rotating left (delta < 0), a positive coeff relaxes the servo to cancel phantom tension.
bool wristCompensationEnabled = true;
float wristCompCoeffs[5] = { -0.10f, -0.05f, 0.00f, 0.08f, 0.15f }; // Thumb, Index, Middle, Ring, Pinky

// Optional Single Servo Direct GPIO Test Pin (if testing without PCA9685)
#define DIRECT_GPIO_PIN     18
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
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_SPEED);

    Wire.beginTransmission(PCA9685_ADDR);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    writeRegister(PCA9685_MODE1, 0x00);
    delay(10);
    setPWMFreq(PWM_FREQ);
    writeRegister(PCA9685_MODE1, 0xA1);
    writeRegister(PCA9685_MODE2, 0x04); // Totem-pole active output drive (OUTDRV = 1)
    delay(5);
    return true;
}

// Forward declaration
void writeAngleHardware(uint8_t channel, float angle, bool updateDependents = true);

void writeAngleHardware(uint8_t channel, float angle, bool updateDependents) {
    if (channel >= NUM_SERVOS) return;
    angle = constrain(angle, 0.0f, 200.0f);
    currentAngles[channel] = angle;

    float effectiveAngle = angle;
    // Apply wrist decoupling compensation to fingers (CH 0-4)
    if (channel < 5 && wristCompensationEnabled) {
        float deltaWrist = currentAngles[5] - 90.0f;
        effectiveAngle += wristCompCoeffs[channel] * deltaWrist;
        effectiveAngle = constrain(effectiveAngle, 0.0f, 200.0f);
    }

    uint16_t pMin = pulseMin[channel];
    uint16_t pMax = pulseMax[channel];
    uint16_t pulse = (uint16_t)map((long)(effectiveAngle * 10.0f), 0, 1800, pMin, pMax);
    pulse = constrain(pulse, (uint16_t)70, (uint16_t)650);
    setPWM(channel, 0, pulse);

    // If wrist moved, update all fingers so tension stays decoupled
    if (channel == 5 && updateDependents && wristCompensationEnabled) {
        for (int ch = 0; ch < 5; ch++) {
            writeAngleHardware(ch, currentAngles[ch], false);
        }
    }
}

void initDirectGPIOServo() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(DIRECT_GPIO_PIN, 50, 14); // Core 3.0+: 50Hz, 14-bit
#else
    ledcSetup(0, 50, 14);                // Core 2.x: Channel 0, 50Hz, 14-bit
    ledcAttachPin(DIRECT_GPIO_PIN, 0);
#endif
    directModeEnabled = true;
}

void setDirectGPIOServo(float angle) {
    angle = constrain(angle, 0.0f, 200.0f);
    uint32_t duty = map((long)(angle * 10.0f), 0, 1800, 410, 2200);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(DIRECT_GPIO_PIN, duty);
#else
    ledcWrite(0, duty);
#endif
}

// ==========================================
// I2C Bus Scanner
// ==========================================
void scanI2CBus() {
    Serial.println("\n[I2C] Scanning bus (SDA=21, SCL=22)...");
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
    }
    if (count == 0) {
        Serial.println("  [ERROR] No I2C devices detected!");
        Serial.println("  Checklist:");
        Serial.println("   1. Verify ESP32 GPIO 21 goes to PCA9685 SDA.");
        Serial.println("   2. Verify ESP32 GPIO 22 goes to PCA9685 SCL.");
        Serial.println("   3. Verify ESP32 3.3V goes to PCA9685 VCC (logic power).");
        Serial.println("   4. Verify common GND is connected between ESP32 and PCA9685.");
    } else {
        Serial.printf("  Scan finished. %d device(s) found.\n", count);
    }
}

// ==========================================
// Test Routines
// ==========================================
void zeroAllServos() {
    Serial.println("\n[*] HOMING ALL 6 SERVOS TO DEFAULT POSITIONS...");
    Serial.println("  -> Fingers (CH 0-4) -> 0 deg (Open Hand)");
    Serial.println("  -> Wrist   (CH 5)   -> 90 deg (Straight / Neutral Center)");
    for (int i = 0; i < 5; i++) {
        writeAngleHardware(i, 0.0f, false);
        delay(70);
    }
    writeAngleHardware(5, 90.0f, true);
    delay(70);
    Serial.println("[*] Done. Fingers at 0 deg, Wrist holding straight at 90 deg.");
    Serial.println("[!] Safe to mount pulleys and align wrist drive gears now.");
}

void sweepChannel(int ch, int delayMs = 15) {
    if (ch < 0 || ch >= NUM_SERVOS) return;

    if (ch == 5) {
        Serial.println("\n[*] Sweeping Wrist (CH 5): Straight(90) -> Left(0) -> Right(180) -> Straight(90)...");
        for (int a = 90; a >= 0; a -= 2) {
            writeAngleHardware(5, a);
            delay(delayMs);
        }
        delay(250);
        for (int a = 0; a <= 180; a += 2) {
            writeAngleHardware(5, a);
            delay(delayMs);
        }
        delay(250);
        for (int a = 180; a >= 90; a -= 2) {
            writeAngleHardware(5, a);
            delay(delayMs);
        }
        Serial.println("[*] Wrist sweep complete. Returned to straight neutral (90 deg).");
        return;
    }

    Serial.printf("\n[*] Sweeping %s (CH %d): 0 -> 180 -> 0 deg...\n", FINGER_NAMES[ch], ch);
    for (int a = 0; a <= 180; a += 2) {
        writeAngleHardware(ch, a);
        delay(delayMs);
    }
    delay(200);
    for (int a = 180; a >= 0; a -= 2) {
        writeAngleHardware(ch, a);
        delay(delayMs);
    }
    Serial.printf("[*] %s sweep complete.\n", FINGER_NAMES[ch]);
}

// 1. Sweep JUST the 5 fingers (CH 0-4) simultaneously while wrist stays at 90 deg
void sweepFingers(int delayMs = 20) {
    Serial.println("\n[*] SWEEPING 5 FINGERS TOGETHER (0 -> 180 -> 0)... [Wrist stays at 90°]");
    for (int a = 0; a <= 180; a += 2) {
        for (int ch = 0; ch < 5; ch++) {
            writeAngleHardware(ch, a, false);
        }
        delay(delayMs);
    }
    delay(400);
    for (int a = 180; a >= 0; a -= 2) {
        for (int ch = 0; ch < 5; ch++) {
            writeAngleHardware(ch, a, false);
        }
        delay(delayMs);
    }
    Serial.println("[*] Fingers sweep complete. All 5 fingers open at 0 deg.");
}

// 2. Stress test: sweep ALL 6 servos together (fingers + wrist)
void sweepAllSimultaneous(int delayMs = 20) {
    Serial.println("\n[*] STRESS TEST: Sweeping ALL 6 servos simultaneously (5 fingers + wrist)...");
    Serial.println("[*] Watch multimeter: SZBK07 buck output should stay rock-solid at 6.00V.");

    for (int a = 0; a <= 180; a += 2) {
        for (int ch = 0; ch < 5; ch++) {
            writeAngleHardware(ch, a, false);
        }
        writeAngleHardware(5, a, false); // Wrist sweeps along 0 -> 180
        delay(delayMs);
    }
    delay(400);
    for (int a = 180; a >= 0; a -= 2) {
        for (int ch = 0; ch < 5; ch++) {
            writeAngleHardware(ch, a, false);
        }
        writeAngleHardware(5, a, false); // Wrist sweeps along 180 -> 0
        delay(delayMs);
    }
    writeAngleHardware(5, 90.0f, true); // Return wrist to straight neutral
    delay(150);
    Serial.println("[*] Simultaneous 6-servo sweep complete. Fingers at 0 deg, Wrist at 90 deg.");
}

void applyPose(const char* name, float t, float i, float m, float r, float p, float w = 90.0f) {
    Serial.printf("\n[*] Applying Pose [%s] (Anti-Snag Collision Avoidance Active)...\n", name);

    // 1. Set wrist first
    writeAngleHardware(5, w, false);

    // 2. Conflict: Index or Middle wants to OPEN (< 50 deg), but Thumb is closed across palm (> 40 deg)
    bool indexOpening = (i < 50.0f && currentAngles[1] > 50.0f);
    bool midOpening   = (m < 50.0f && currentAngles[2] > 50.0f);
    bool thumbClamped = (currentAngles[0] > 40.0f);

    if ((indexOpening || midOpening) && thumbClamped) {
        Serial.println("  -> [SAFE-SEQ] Unclamping Thumb to 30 deg first to prevent index/middle snag...");
        for (float ang = currentAngles[0]; ang >= 30.0f; ang -= 5.0f) {
            writeAngleHardware(0, ang, false);
            delay(15);
        }
        writeAngleHardware(0, 30.0f, false);
        delay(80);

        Serial.println("  -> [SAFE-SEQ] Extending fingers freely past clearance zone...");
        float maxFinger = max(currentAngles[1], currentAngles[2]);
        for (float ang = maxFinger; ang >= 0.0f; ang -= 5.0f) {
            if (indexOpening && ang >= i) writeAngleHardware(1, ang, false);
            if (midOpening && ang >= m)   writeAngleHardware(2, ang, false);
            delay(15);
        }
        writeAngleHardware(1, i, false);
        writeAngleHardware(2, m, false);
        delay(60);

        // Move Ring and Pinky
        writeAngleHardware(3, r, false);
        writeAngleHardware(4, p, false);

        // Fold Thumb to target if desired
        if (t > 40.0f) {
            Serial.println("  -> [SAFE-SEQ] Folding Thumb to target over curled fingers...");
            for (float ang = currentAngles[0]; ang <= t; ang += 5.0f) {
                writeAngleHardware(0, ang, false);
                delay(15);
            }
        }
        writeAngleHardware(0, t, false);
        Serial.printf("[*] Pose [%s] applied cleanly without tension stress.\n", name);
        return;
    }

    // 3. Conflict: Forming a FIST or fingers tightening while Thumb is already closed
    bool fingersClosing = (i > 100.0f || m > 100.0f);
    bool thumbClosing   = (t > 100.0f);
    if (fingersClosing && thumbClosing) {
        // If thumb is already folded into palm, yield it to 30 deg so fingers can curl in first
        if (currentAngles[0] > 40.0f && (currentAngles[1] < 85.0f || currentAngles[2] < 85.0f)) {
            Serial.println("  -> [SAFE-SEQ] Thumb is parked in palm! Yielding Thumb to 30 deg so fingers can curl in...");
            for (float ang = currentAngles[0]; ang >= 30.0f; ang -= 5.0f) {
                writeAngleHardware(0, ang, false);
                delay(15);
            }
            writeAngleHardware(0, 30.0f, false);
            delay(60);
        }

        Serial.println("  -> [SAFE-SEQ] Curling fingers into palm first...");
        float maxFinger = max(max(currentAngles[1], currentAngles[2]), max(currentAngles[3], currentAngles[4]));
        for (float ang = maxFinger; ang <= 180.0f; ang += 5.0f) {
            if (currentAngles[1] < i) writeAngleHardware(1, min(ang, i), false);
            if (currentAngles[2] < m) writeAngleHardware(2, min(ang, m), false);
            if (currentAngles[3] < r) writeAngleHardware(3, min(ang, r), false);
            if (currentAngles[4] < p) writeAngleHardware(4, min(ang, p), false);
            delay(15);
        }
        writeAngleHardware(1, i, false);
        writeAngleHardware(2, m, false);
        writeAngleHardware(3, r, false);
        writeAngleHardware(4, p, false);
        delay(80);

        Serial.println("  -> [SAFE-SEQ] Wrapping Thumb outside curled fingers...");
        for (float ang = currentAngles[0]; ang <= t; ang += 5.0f) {
            writeAngleHardware(0, ang, false);
            delay(15);
        }
        writeAngleHardware(0, t, false);
        Serial.printf("[*] Pose [%s] applied cleanly (Thumb wrapped outside, full tightness).\n", name);
        return;
    }

    // Default: smooth transition
    writeAngleHardware(1, i, false);
    writeAngleHardware(2, m, false);
    writeAngleHardware(3, r, false);
    writeAngleHardware(4, p, false);
    writeAngleHardware(0, t, false);
    Serial.printf("[*] Pose [%s] applied.\n", name);
}

void printStatus() {
    Serial.println("\n=========================================================================================");
    Serial.println("                              KRYPTON SERVO TESTER - STATUS                              ");
    Serial.println("=========================================================================================");
    Serial.printf("Active Channel: %d (%s) | Wrist Tension Compensation: [%s]\n",
                  activeChannel, FINGER_NAMES[activeChannel], wristCompensationEnabled ? "ENABLED" : "DISABLED");
    Serial.println("Current Positions, Home Angles, 12-bit Pulse Calibration & Decoupling Coeffs:");
    Serial.println(" CH | Joint/Name | Angle (deg) | Home / Neutral | PulseMin | PulseMax | Approx Max us | Wrist Comp");
    Serial.println("----+------------+-------------+----------------+----------+----------+---------------+-----------");
    for (int i = 0; i < NUM_SERVOS; i++) {
        const char* homeLabel = (i == 5) ? " 90 deg (Mid)" : "  0 deg (Open)";
        uint16_t usMax = (uint16_t)(pulseMax[i] * 4.8828f);
        char compStr[16];
        if (i < 5) snprintf(compStr, sizeof(compStr), "%+.2f", wristCompCoeffs[i]);
        else snprintf(compStr, sizeof(compStr), " [Driver]");

        Serial.printf("  %d | %-10s |  %5.1f deg   | %-14s |   %3d    |   %3d    |   ~%4d us     |  %s %s\n",
                      i, FINGER_NAMES[i], currentAngles[i], homeLabel, pulseMin[i], pulseMax[i], usMax, compStr,
                      (i == activeChannel) ? "<-- [ACTIVE]" : "");
    }
    Serial.println("=========================================================================================");
    Serial.printf("Wrist Delta from Neutral: %+.1f deg (Affects finger string path length)\n", currentAngles[5] - 90.0f);
    Serial.println("[TIP] Type 'w' to sweep just fingers. Type 'wa' to sweep all 6 together.");
    Serial.println("[TIP] Type '+' or 'max 555' to tighten loose fingers. Type 'comp on/off' to toggle decoupling.");
    Serial.println("=========================================================================================");
}

void printMenu() {
    Serial.println("\n-----------------------------------------------------------------------------------------");
    Serial.println("                                  AVAILABLE COMMANDS                                     ");
    Serial.println("-----------------------------------------------------------------------------------------");
    Serial.println("  0 - 5        : Select active channel (0:Thumb, 1:Idx, 2:Mid, 3:Ring, 4:Pinky, 5:Wrist)");
    Serial.println("  <angle>      : Move active channel to angle 0-200 (e.g. '0', '90', '180')");
    Serial.println("  <ch> <angle> : Move specified channel directly (e.g. '1 180', 'wrist 90')");
    Serial.println("  all <angle>  : Move ALL 6 channels to angle (e.g. 'all 180', 'all 0')");
    Serial.println("  center / c   : Move wrist to straight neutral (90 deg)");
    Serial.println("  left / l     : Rotate wrist left (0 deg)");
    Serial.println("  right / r    : Rotate wrist right (180 deg)");
    Serial.println("  s            : Sweep active channel (Fingers: 0->180->0, Wrist: 90->0->180->90)");
    Serial.println("  w  or wf     : SWEEP 5 FINGERS ONLY (Wrist holds steady at 90 deg neutral)");
    Serial.println("  wa or wall   : STRESS TEST: SWEEP ALL 6 SERVOS TOGETHER (5 fingers + wrist)");
    Serial.println("  z            : Home/Zero (Fingers -> 0 deg, Wrist -> 90 deg straight)");
    Serial.println("  p            : Cycle through preset poses (Fist, Peace, Wrist L/R, etc.)");
    Serial.println("  +  or +5     : TIGHTEN: Increase pulseMax by 5 ticks (tightens loose finger at 180)");
    Serial.println("  -  or -5     : RELIEVE: Decrease pulseMax by 5 ticks (stops motor buzz/hum)");
    Serial.println("  +10 / -10    : Adjust active finger pulseMax by 10 ticks");
    Serial.println("  max <ticks>  : Set max pulse ticks for active channel (e.g. 'max 555')");
    Serial.println("  min <ticks>  : Set min pulse ticks for active channel (e.g. 'min 102')");
    Serial.println("  pulse <ticks>: Send raw PWM ticks directly to servo (e.g. 'pulse 545')");
    Serial.println("  trim <min> <max> : Set both pulse limits for active channel (e.g. 'trim 102 550')");
    Serial.println("  comp         : View wrist decoupling compensation coefficients");
    Serial.println("  comp on/off  : Enable or disable automatic wrist-tendon tension compensation");
    Serial.println("  comp <ch> <k>: Set decoupling factor for finger (e.g. 'comp 4 0.15' or 'comp pinky 0.18')");
    Serial.println("  scan         : Re-run I2C bus scanner");
    Serial.println("  status       : Print live angles, pulse limits, and compensation table");
    Serial.println("  gpio <angle> : Test direct ESP32 GPIO 18 (without PCA9685)");
    Serial.println("  ? or help    : Show this help menu");
    Serial.println("-----------------------------------------------------------------------------------------");
}

// ==========================================
// Command Parser
// ==========================================
int poseIndex = 0;
void cyclePose() {
    switch (poseIndex) {
        case 0: applyPose("OPEN HAND (WRIST STRAIGHT)", 0, 0, 0, 0, 0, 90); break;
        case 1: applyPose("FULL FIST (WRIST STRAIGHT)", 180, 180, 180, 180, 180, 90); break;
        case 2: applyPose("POINT (WRIST STRAIGHT)", 180, 0, 180, 180, 180, 90); break;
        case 3: applyPose("PEACE (V) (WRIST STRAIGHT)", 180, 0, 0, 180, 180, 90); break;
        case 4: applyPose("THUMBS UP (WRIST STRAIGHT)", 0, 180, 180, 180, 180, 90); break;
        case 5: applyPose("ROCK ON (WRIST STRAIGHT)", 0, 0, 180, 180, 0, 90); break;
        case 6: applyPose("WRIST ROTATED LEFT", 0, 0, 0, 0, 0, 0); break;
        case 7: applyPose("WRIST ROTATED RIGHT", 0, 0, 0, 0, 0, 180); break;
        case 8: applyPose("WRIST STRAIGHT / CENTER", 0, 0, 0, 0, 0, 90); break;
    }
    poseIndex = (poseIndex + 1) % 9;
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
    } else if (cmd.equalsIgnoreCase("z") || cmd.equalsIgnoreCase("zero") || cmd.equalsIgnoreCase("home")) {
        zeroAllServos();
    } else if (cmd.equalsIgnoreCase("s") || cmd.equalsIgnoreCase("sweep")) {
        sweepChannel(activeChannel);
    // User requested: "1 command that will just use 'w' cycle for just fingers, and the 1 which we have rn for all together"
    } else if (cmd.equalsIgnoreCase("w") || cmd.equalsIgnoreCase("wf") || cmd.equalsIgnoreCase("fingers") || cmd.equalsIgnoreCase("sweepfingers")) {
        sweepFingers();
    } else if (cmd.equalsIgnoreCase("wa") || cmd.equalsIgnoreCase("wall") || cmd.equalsIgnoreCase("alltogether") || cmd.equalsIgnoreCase("stress") || cmd.equalsIgnoreCase("sweepall")) {
        sweepAllSimultaneous();
    } else if (cmd.equalsIgnoreCase("p") || cmd.equalsIgnoreCase("pose")) {
        cyclePose();
    } else if (cmd.equalsIgnoreCase("c") || cmd.equalsIgnoreCase("center") || cmd.equalsIgnoreCase("straight")) {
        writeAngleHardware(5, 90.0f);
        Serial.println("[*] Wrist moved to 90.0 deg (Straight / Neutral Center).");
    } else if (cmd.equalsIgnoreCase("left") || cmd.equalsIgnoreCase("l")) {
        writeAngleHardware(5, 0.0f);
        Serial.println("[*] Wrist moved to 0.0 deg (Left Rotation).");
    } else if (cmd.equalsIgnoreCase("right") || cmd.equalsIgnoreCase("r")) {
        writeAngleHardware(5, 180.0f);
        Serial.println("[*] Wrist moved to 180.0 deg (Right Rotation).");
    } else if (cmd.equalsIgnoreCase("comp on")) {
        wristCompensationEnabled = true;
        Serial.println("[*] Wrist Tension Decoupling Compensation: ENABLED.");
        writeAngleHardware(5, currentAngles[5]); // Refresh finger offsets
    } else if (cmd.equalsIgnoreCase("comp off")) {
        wristCompensationEnabled = false;
        Serial.println("[*] Wrist Tension Decoupling Compensation: DISABLED.");
        for (int i = 0; i < 5; i++) writeAngleHardware(i, currentAngles[i], false);
    } else if (cmd.equalsIgnoreCase("comp")) {
        Serial.println("\n[*] Current Wrist-Tendon Compensation Coefficients:");
        for (int i = 0; i < 5; i++) {
            Serial.printf("  CH %d (%-7s): %+.2f\n", i, FINGER_NAMES[i], wristCompCoeffs[i]);
        }
        Serial.printf("Status: %s (Wrist Delta: %+.1f deg)\n",
                      wristCompensationEnabled ? "ENABLED" : "DISABLED", currentAngles[5] - 90.0f);
    } else if (cmd.startsWith("comp ") || cmd.startsWith("COMP ")) {
        // Syntax: comp <channel/name> <coeff> (e.g. 'comp 4 0.18' or 'comp pinky 0.15')
        int firstSpace = cmd.indexOf(' ');
        int secondSpace = cmd.indexOf(' ', firstSpace + 1);
        if (secondSpace != -1) {
            String chStr = cmd.substring(firstSpace + 1, secondSpace);
            float k = cmd.substring(secondSpace + 1).toFloat();
            int ch = -1;
            if (chStr.length() == 1 && chStr[0] >= '0' && chStr[0] <= '4') ch = chStr[0] - '0';
            else if (chStr.equalsIgnoreCase("thumb")) ch = 0;
            else if (chStr.equalsIgnoreCase("index")) ch = 1;
            else if (chStr.equalsIgnoreCase("middle") || chStr.equalsIgnoreCase("mid")) ch = 2;
            else if (chStr.equalsIgnoreCase("ring")) ch = 3;
            else if (chStr.equalsIgnoreCase("pinky")) ch = 4;

            if (ch >= 0 && ch < 5) {
                wristCompCoeffs[ch] = k;
                Serial.printf("[*] Updated %s (CH %d) wrist compensation coeff -> %+.3f\n", FINGER_NAMES[ch], ch, k);
                writeAngleHardware(5, currentAngles[5]); // Refresh
            } else {
                Serial.println("[ERROR] Invalid finger channel. Choose 0 to 4 (Thumb to Pinky).");
            }
        } else {
            Serial.println("[ERROR] Syntax: comp <channel> <coeff> (e.g. 'comp 4 0.15' or 'comp pinky 0.18')");
        }
    } else if (cmd == "+" || cmd == "+5") {
        pulseMax[activeChannel] = min((int)pulseMax[activeChannel] + 5, 650);
        Serial.printf("[*] %s (CH %d) pulseMax tightened -> %d ticks (~%d us)\n",
                      FINGER_NAMES[activeChannel], activeChannel, pulseMax[activeChannel],
                      (int)(pulseMax[activeChannel] * 4.8828f));
        writeAngleHardware(activeChannel, currentAngles[activeChannel]);
    } else if (cmd == "+10") {
        pulseMax[activeChannel] = min((int)pulseMax[activeChannel] + 10, 650);
        Serial.printf("[*] %s (CH %d) pulseMax tightened -> %d ticks (~%d us)\n",
                      FINGER_NAMES[activeChannel], activeChannel, pulseMax[activeChannel],
                      (int)(pulseMax[activeChannel] * 4.8828f));
        writeAngleHardware(activeChannel, currentAngles[activeChannel]);
    } else if (cmd == "-" || cmd == "-5") {
        pulseMax[activeChannel] = max((int)pulseMax[activeChannel] - 5, (int)pulseMin[activeChannel] + 30);
        Serial.printf("[*] %s (CH %d) pulseMax relieved -> %d ticks (~%d us)\n",
                      FINGER_NAMES[activeChannel], activeChannel, pulseMax[activeChannel],
                      (int)(pulseMax[activeChannel] * 4.8828f));
        writeAngleHardware(activeChannel, currentAngles[activeChannel]);
    } else if (cmd == "-10") {
        pulseMax[activeChannel] = max((int)pulseMax[activeChannel] - 10, (int)pulseMin[activeChannel] + 30);
        Serial.printf("[*] %s (CH %d) pulseMax relieved -> %d ticks (~%d us)\n",
                      FINGER_NAMES[activeChannel], activeChannel, pulseMax[activeChannel],
                      (int)(pulseMax[activeChannel] * 4.8828f));
        writeAngleHardware(activeChannel, currentAngles[activeChannel]);
    } else if (cmd.startsWith("max ") || cmd.startsWith("MAX ")) {
        uint16_t val = cmd.substring(4).toInt();
        if (val >= pulseMin[activeChannel] + 40 && val <= 650) {
            pulseMax[activeChannel] = val;
            Serial.printf("[*] %s (CH %d) pulseMax set -> %d ticks (~%d us)\n",
                          FINGER_NAMES[activeChannel], activeChannel, val, (int)(val * 4.8828f));
            writeAngleHardware(activeChannel, currentAngles[activeChannel]);
        } else {
            Serial.println("[ERROR] Invalid max pulse. Allowed range: 450 to 650 ticks.");
        }
    } else if (cmd.startsWith("min ") || cmd.startsWith("MIN ")) {
        uint16_t val = cmd.substring(4).toInt();
        if (val >= 60 && val <= pulseMax[activeChannel] - 40) {
            pulseMin[activeChannel] = val;
            Serial.printf("[*] %s (CH %d) pulseMin set -> %d ticks (~%d us)\n",
                          FINGER_NAMES[activeChannel], activeChannel, val, (int)(val * 4.8828f));
            writeAngleHardware(activeChannel, currentAngles[activeChannel]);
        } else {
            Serial.println("[ERROR] Invalid min pulse. Allowed range: 60 to 200 ticks.");
        }
    } else if (cmd.startsWith("pulse ") || cmd.startsWith("PULSE ") || cmd.startsWith("raw ") || cmd.startsWith("RAW ")) {
        int spaceIdx = cmd.indexOf(' ');
        uint16_t val = cmd.substring(spaceIdx + 1).toInt();
        if (val >= 60 && val <= 650) {
            setPWM(activeChannel, 0, val);
            Serial.printf("[*] %s (CH %d) RAW PWM pulse output: %d ticks (~%d us)\n",
                          FINGER_NAMES[activeChannel], activeChannel, val, (int)(val * 4.8828f));
        } else {
            Serial.println("[ERROR] Allowed raw pulse range: 60 to 650 ticks.");
        }
    } else if (cmd.length() == 1 && cmd[0] >= '0' && cmd[0] <= '5') {
        activeChannel = cmd[0] - '0';
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else if (cmd.equalsIgnoreCase("wrist")) {
        activeChannel = 5;
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else if (cmd.equalsIgnoreCase("thumb")) {
        activeChannel = 0;
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else if (cmd.equalsIgnoreCase("index")) {
        activeChannel = 1;
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else if (cmd.equalsIgnoreCase("middle") || cmd.equalsIgnoreCase("mid")) {
        activeChannel = 2;
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else if (cmd.equalsIgnoreCase("ring")) {
        activeChannel = 3;
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else if (cmd.equalsIgnoreCase("pinky")) {
        activeChannel = 4;
        Serial.printf("[*] Selected Active Channel: %d (%s)\n", activeChannel, FINGER_NAMES[activeChannel]);
    } else if (cmd.startsWith("all ") || cmd.startsWith("ALL ")) {
        float ang = cmd.substring(4).toFloat();
        ang = constrain(ang, 0.0f, 200.0f);
        Serial.printf("[*] Moving ALL 6 channels to %.1f deg\n", ang);
        for (int i = 0; i < NUM_SERVOS; i++) {
            writeAngleHardware(i, ang, false);
        }
    } else if (cmd.startsWith("trim ") || cmd.startsWith("TRIM ")) {
        int spaceIdx = cmd.indexOf(' ', 5);
        if (spaceIdx != -1) {
            uint16_t nMin = cmd.substring(5, spaceIdx).toInt();
            uint16_t nMax = cmd.substring(spaceIdx + 1).toInt();
            if (nMin >= 60 && nMin < nMax && nMax <= 650) {
                pulseMin[activeChannel] = nMin;
                pulseMax[activeChannel] = nMax;
                Serial.printf("[*] %s (CH %d) pulse trim updated: Min=%d, Max=%d\n",
                              FINGER_NAMES[activeChannel], activeChannel, nMin, nMax);
                writeAngleHardware(activeChannel, currentAngles[activeChannel]);
            } else {
                Serial.println("[ERROR] Invalid pulse range. Allowed: Min 60-200, Max 450-650.");
            }
        } else {
            Serial.println("[ERROR] Syntax: trim <min> <max> (e.g. 'trim 102 550')");
        }
    } else if (cmd.startsWith("gpio ") || cmd.startsWith("GPIO ")) {
        float ang = cmd.substring(5).toFloat();
        if (!directModeEnabled) {
            initDirectGPIOServo();
            Serial.printf("[*] Initialized direct ESP32 GPIO %d PWM.\n", DIRECT_GPIO_PIN);
        }
        setDirectGPIOServo(ang);
        Serial.printf("[*] Direct GPIO %d commanded to %.1f deg.\n", DIRECT_GPIO_PIN, ang);
    } else {
        // Check for "<channel> <angle>" syntax (e.g. "5 90", "wrist 90", "1 180")
        int spaceIdx = cmd.indexOf(' ');
        if (spaceIdx != -1) {
            String chStr = cmd.substring(0, spaceIdx);
            String angStr = cmd.substring(spaceIdx + 1);
            int ch = -1;
            if (chStr.length() == 1 && chStr[0] >= '0' && chStr[0] <= '5') ch = chStr[0] - '0';
            else if (chStr.equalsIgnoreCase("wrist")) ch = 5;
            else if (chStr.equalsIgnoreCase("thumb")) ch = 0;
            else if (chStr.equalsIgnoreCase("index")) ch = 1;
            else if (chStr.equalsIgnoreCase("middle") || chStr.equalsIgnoreCase("mid")) ch = 2;
            else if (chStr.equalsIgnoreCase("ring")) ch = 3;
            else if (chStr.equalsIgnoreCase("pinky")) ch = 4;

            if (ch >= 0 && ch < NUM_SERVOS) {
                float ang = angStr.toFloat();
                if (ang >= 0.0f && ang <= 200.0f) {
                    activeChannel = ch;
                    writeAngleHardware(ch, ang);
                    Serial.printf("[*] %s (CH %d) set to %.1f deg\n", FINGER_NAMES[ch], ch, ang);
                    return;
                }
            }
        }

        // Assume single numeric angle for active channel (e.g. 0 to 200)
        float ang = cmd.toFloat();
        if (ang >= 0.0f && ang <= 200.0f) {
            writeAngleHardware(activeChannel, ang);
            Serial.printf("[*] %s (CH %d) set to %.1f deg\n", FINGER_NAMES[activeChannel], activeChannel, ang);
        } else {
            Serial.println("[?] Unknown command. Type 'help' or '?' for available commands.");
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
    Serial.println("=========================================================================");
    Serial.println("                 KRYPTON BIONIC ARM - SERVO TEST SUITE                   ");
    Serial.println("=========================================================================");
    Serial.println("CH 0-4: Fingers (0=Open, 180=Tight Fist)");
    Serial.println("CH 5  : Wrist   (90=Straight Neutral, 0=Left, 180=Right)");
    Serial.println("Calibrated 180° Pulse Limits: Index=550 (+10), Middle=545 (+5)");
    Serial.println("Wrist Tendon Decoupling: Active (Tension compensated across rotation)");
    Serial.println("Designed for 12V Battery + SZBK07 20A Buck + PCA9685");

    // Initialize I2C and check PCA9685
    Serial.print("[INIT] Connecting to PCA9685 at 0x40... ");
    if (initPCA9685()) {
        Serial.println("SUCCESS (50Hz PWM Ready)");
        // Safely home all servos on boot (Fingers -> 0 deg, Wrist -> 90 deg)
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
}
