/*
 * Krypton Bionic Arm - Direct ESP8266 Single Servo Bench Tester
 * 
 * Purpose:
 * Tests a single TowerPro MG996R servo directly from ESP8266 pin D5 (GPIO 14)
 * using the built-in Servo library.
 * Zero I2C, zero PCA9685. Tests servos and pulleys directly.
 * 
 * Hardware Wiring:
 * 1. ESP8266 USB connects to PC (powers ESP8266 and provides Serial Monitor).
 * 2. Servo Yellow / Orange (PWM Signal Wire) -> ESP8266 Pin D5 (GPIO 14).
 * 3. Servo Red (+5V / +6V Power Wire)         -> Buck Converter OUT+ (6.0V) or 5V Power Supply (+).
 * 4. Servo Brown / Black (Ground Wire)        -> Buck Converter OUT- (GND) AND ESP8266 GND pin!
 *    (CRITICAL: The power supply negative and ESP8266 GND MUST be connected together!)
 * 
 * Serial Monitor:
 * Baud Rate: 115200 (set "Newline" or "Both NL & CR" in Arduino IDE)
 */

#include <Arduino.h>
#include <Servo.h>

#define SERVO_PIN 14 // Pin D5 (GPIO 14 on NodeMCU / Wemos D1 Mini)

Servo myServo;
float currentAngle = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==================================================");
    Serial.println("   KRYPTON - ESP8266 DIRECT SERVO BENCH TESTER    ");
    Serial.println("==================================================");
    Serial.println("Connected Pin: D5 (GPIO 14)");

    // Attach servo with standard 500us to 2500us pulse limits for MG996R
    myServo.attach(SERVO_PIN, 500, 2500);

    // Starts servo at 0 degrees (open hand position)
    myServo.write(0);
    currentAngle = 0;
    delay(100);

    Serial.println("[*] Servo initialized and homed to 0 degrees.");
    Serial.println("[!] Safe to mount yellow winch pulley now.");
    Serial.println();
    Serial.println("Available Commands:");
    Serial.println("  <angle> : Enter any number from 0 to 180 (e.g. '90', '180', '0')");
    Serial.println("  s       : Smooth sweep (0 -> 180 -> 0 degrees)");
    Serial.println("  z       : Reset back to 0 degrees");
    Serial.println("--------------------------------------------------");
}

void sweep() {
    Serial.println("\n[*] Sweeping: 0 -> 180 -> 0 degrees...");
    for (int a = 0; a <= 180; a += 2) {
        myServo.write(a);
        currentAngle = a;
        delay(15);
        yield();
    }
    delay(200);
    yield();
    for (int a = 180; a >= 0; a -= 2) {
        myServo.write(a);
        currentAngle = a;
        delay(15);
        yield();
    }
    Serial.println("[*] Sweep complete. Held at 0 degrees.");
}

void loop() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() == 0) return;

        if (input.equalsIgnoreCase("s") || input.equalsIgnoreCase("sweep")) {
            sweep();
        } else if (input.equalsIgnoreCase("z") || input.equalsIgnoreCase("zero")) {
            myServo.write(0);
            currentAngle = 0;
            Serial.println("[*] Servo moved to 0 degrees.");
        } else if (input == "?" || input.equalsIgnoreCase("help")) {
            Serial.println("Commands: Enter an angle ('0', '90', '180'), 's' to sweep, or 'z' to zero.");
        } else {
            float ang = input.toFloat();
            if (ang >= 0 && ang <= 180) {
                myServo.write((int)ang);
                currentAngle = ang;
                Serial.printf("[*] Commanded servo to %.0f degrees.\n", ang);
            } else {
                Serial.println("[?] Please enter a valid angle between 0 and 180.");
            }
        }
    }
    yield();
}
