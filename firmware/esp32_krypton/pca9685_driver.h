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
            _current_angles[i] = SERVO_HOME_ANGLES[i];
            _target_angles[i]  = SERVO_HOME_ANGLES[i];
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
        writeRegister(PCA9685_MODE2, 0x04); // Totem-pole active drive (OUTDRV = 1)
        delay(5);

        // Home all servos to default positions (Fingers -> 0 deg, Wrist -> 90 deg)
        for (int i = 0; i < NUM_SERVOS; i++) {
            setServoAngle(i, SERVO_HOME_ANGLES[i], true);
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
        writeRegister(PCA9685_MODE1, oldmode | 0xA1); // Restart and auto-increment
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
            writeAngleHardware(channel, angle, true);
        }
    }

    void updateTrajectory() {
        if (!MOTION_SMOOTHING) return;

        unsigned long now = millis();
        if (now - _last_update_ms < TRAJECTORY_INTERVAL_MS) return;
        _last_update_ms = now;

        // Compute effective intermediate setpoints with collision arbitration
        float effective_targets[NUM_SERVOS];
        for (int i = 0; i < NUM_SERVOS; i++) {
            effective_targets[i] = _target_angles[i];
        }

#if ENABLE_COLLISION_AVOIDANCE
        float thumbCur = _current_angles[CH_THUMB];
        float indexCur = _current_angles[CH_INDEX];
        float indexTgt = _target_angles[CH_INDEX];
        float thumbTgt = _target_angles[CH_THUMB];

        // 1. Conflict: Index wants to OPEN (< 45 deg) while Thumb is currently clamped across the palm (> 35 deg)
        // Yield thumb outward first so index has a free swing path without tension snag
        if (indexTgt < FINGER_CLEARANCE_ANGLE && indexCur > FINGER_CLEARANCE_ANGLE && thumbCur > THUMB_CLEARANCE_ANGLE) {
            effective_targets[CH_THUMB] = THUMB_CLEARANCE_ANGLE;
            // Hold index from pulling hard against clamped thumb until thumb clears past 42 deg
            if (thumbCur > THUMB_CLEARANCE_ANGLE + 7.0f) {
                effective_targets[CH_INDEX] = max(indexCur, COLLISION_ZONE_MIN);
            }
        }
        // 2. Conflict: Index is actively CLOSING into the palm (< 65 deg currently, targeting > 100 deg)
        // while Thumb is already closed across the palm (> 35 deg).
        // Yield thumb briefly so index can curl inside the palm first, then Thumb clamps outside tightly
        else if (indexTgt > 100.0f && indexCur < 65.0f && thumbCur > THUMB_CLEARANCE_ANGLE) {
            effective_targets[CH_THUMB] = THUMB_CLEARANCE_ANGLE;
        }
#endif

        for (int i = 0; i < NUM_SERVOS; i++) {
            float diff = effective_targets[i] - _current_angles[i];
            if (abs(diff) > 0.1f) {
                if (abs(diff) <= TRAJECTORY_STEP_DEG) {
                    _current_angles[i] = effective_targets[i];
                } else {
                    _current_angles[i] += (diff > 0 ? TRAJECTORY_STEP_DEG : -TRAJECTORY_STEP_DEG);
                }
                writeAngleHardware(i, _current_angles[i], true);
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

    void writeAngleHardware(uint8_t channel, float angle, bool updateDependents = true) {
        if (channel >= NUM_SERVOS) return;

        if (SERVO_INVERT[channel]) {
            angle = 180.0f - angle;
        }

        float effectiveAngle = angle;
#if ENABLE_WRIST_COMPENSATION
        // Decouple wrist rotation from finger tendon path length
        if (channel < 5) {
            float deltaWrist = _current_angles[CH_WRIST] - 90.0f;
            effectiveAngle += WRIST_COMP_COEFFS[channel] * deltaWrist;
            effectiveAngle = constrain(effectiveAngle, (float)SERVO_MIN_ANGLES[channel], (float)SERVO_MAX_ANGLES[channel]);
        }
#endif

        // Map degrees (0-180) to 12-bit PWM ticks using channel-specific calibrated pulse limits
        uint16_t minPulse = SERVO_PULSE_MIN_MAP[channel];
        uint16_t maxPulse = SERVO_PULSE_MAX_MAP[channel];
        uint16_t pulse = (uint16_t)map((long)(effectiveAngle * 10.0f), 0, 1800, minPulse, maxPulse);
        pulse = constrain(pulse, (uint16_t)70, (uint16_t)650);
        setPWM(channel, 0, pulse);

#if ENABLE_WRIST_COMPENSATION
        // If wrist moved, update all fingers so tension stays decoupled in real time
        if (channel == CH_WRIST && updateDependents) {
            for (int i = 0; i < 5; i++) {
                writeAngleHardware(i, _current_angles[i], false);
            }
        }
#endif
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
