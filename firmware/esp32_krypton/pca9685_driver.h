#ifndef PCA9685_DRIVER_H
#define PCA9685_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// Register Addresses for PCA9685
#define PCA9685_MODE1       0x00
#define PCA9685_MODE2       0x01
#define PCA9685_PRESCALE    0xFE
#define PCA9685_LED0_ON_L   0x06

class PCA9685Driver {
public:
    PCA9685Driver(uint8_t addr = PCA9685_I2C_ADDR) : _addr(addr) {
        for (int i = 0; i < NUM_SERVOS; i++) {
            _current_angles[i] = 0.0f;
            _target_angles[i] = 0.0f;
        }
        _last_update_ms = 0;
    }

    bool begin() {
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_SPEED);
        
        // Reset device
        writeRegister(PCA9685_MODE1, 0x00);
        delay(10);

        // Set PWM Frequency
        setPWMFreq(PWM_FREQUENCY);

        // Wake up oscillator & enable auto-increment
        writeRegister(PCA9685_MODE1, 0xA1);
        delay(5);

        // Set default 0 degrees (open hand)
        for (int i = 0; i < NUM_SERVOS; i++) {
            setServoAngle(i, 0.0f, true);
        }

        return true;
    }

    void setPWMFreq(float freq) {
        float prescaleval = 25000000.0f; // 25MHz internal clock
        prescaleval /= 4096.0f;
        prescaleval /= freq;
        prescaleval -= 1.0f;
        uint8_t prescale = (uint8_t)(prescaleval + 0.5f);

        uint8_t oldmode = readRegister(PCA9685_MODE1);
        uint8_t newmode = (oldmode & 0x7F) | 0x10; // Sleep mode to set prescaler
        writeRegister(PCA9685_MODE1, newmode);
        writeRegister(PCA9685_PRESCALE, prescale);
        writeRegister(PCA9685_MODE1, oldmode);
        delay(5);
        writeRegister(PCA9685_MODE1, oldmode | 0xa1); // Restart and auto-increment
    }

    void setPWM(uint8_t channel, uint16_t on, uint16_t off) {
        Wire.beginTransmission(_addr);
        Wire.write(PCA9685_LED0_ON_L + 4 * channel);
        Wire.write(on & 0xFF);
        Wire.write(on >> 8);
        Wire.write(off & 0xFF);
        Wire.write(off >> 8);
        Wire.endTransmission();
    }

    void setServoAngle(uint8_t channel, float angle, bool immediate = false) {
        if (channel >= NUM_SERVOS) return;

        // Clamp to allowed mechanical range
        angle = constrain(angle, (float)SERVO_MIN_ANGLES[channel], (float)SERVO_MAX_ANGLES[channel]);

        _target_angles[channel] = angle;
        if (immediate || !MOTION_SMOOTHING) {
            _current_angles[channel] = angle;
            writeAngleHardware(channel, angle);
        }
    }

    void updateTrajectory() {
        if (!MOTION_SMOOTHING) return;

        unsigned long now = millis();
        if (now - _last_update_ms < TRAJECTORY_INTERVAL_MS) return;
        _last_update_ms = now;

        for (int i = 0; i < NUM_SERVOS; i++) {
            float diff = _target_angles[i] - _current_angles[i];
            if (abs(diff) > 0.1f) {
                if (abs(diff) <= TRAJECTORY_STEP_DEG) {
                    _current_angles[i] = _target_angles[i];
                } else {
                    _current_angles[i] += (diff > 0 ? TRAJECTORY_STEP_DEG : -TRAJECTORY_STEP_DEG);
                }
                writeAngleHardware(i, _current_angles[i]);
            }
        }
    }

    float getCurrentAngle(uint8_t channel) const {
        if (channel >= NUM_SERVOS) return 0.0f;
        return _current_angles[channel];
    }

    float getTargetAngle(uint8_t channel) const {
        if (channel >= NUM_SERVOS) return 0.0f;
        return _target_angles[channel];
    }

private:
    uint8_t _addr;
    float _current_angles[NUM_SERVOS];
    float _target_angles[NUM_SERVOS];
    unsigned long _last_update_ms;

    void writeAngleHardware(uint8_t channel, float angle) {
        if (SERVO_INVERT[channel]) {
            angle = 180.0f - angle;
        }

        // Map degrees (0-180) to 12-bit PWM ticks (SERVO_PULSE_MIN to SERVO_PULSE_MAX)
        uint16_t pulse = (uint16_t)map((long)(angle * 10.0f), 0, 1800, SERVO_PULSE_MIN, SERVO_PULSE_MAX);
        setPWM(channel, 0, pulse);
    }

    void writeRegister(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        Wire.write(val);
        Wire.endTransmission();
    }

    uint8_t readRegister(uint8_t reg) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        Wire.endTransmission();
        Wire.requestFrom((uint8_t)_addr, (uint8_t)1);
        return Wire.read();
    }
};

#endif // PCA9685_DRIVER_H
