#include <Arduino.h>
#include <Wire.h>
#include "config.h"

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_SPEED);
    Serial.println("[TEST] Scanning I2C bus...");
    Wire.beginTransmission(PCA9685_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("[TEST] PCA9685 detected at 0x40!");
    } else {
        Serial.println("[TEST] PCA9685 not detected.");
    }
}

void loop() {
    delay(1000);
}
