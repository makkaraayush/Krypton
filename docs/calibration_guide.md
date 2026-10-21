# Calibration & Tuning Guide

Achieving smooth, lifelike movement requires electrical, mechanical, and software calibration.

---

## 1. Electrical Calibration

Always verify voltages with a multimeter before powering electronics:
- **Buck Converter Output**: Must read **6.0V DC +/- 0.05V**.
- **PCA9685 V+**: Must measure 6.0V.
- **PCA9685 VCC**: Must measure 3.3V (from ESP32 3.3V rail).
- **ESP32 VIN**: Receives 6.0V (onboard LDO regulates to 3.3V).

---

## 2. Software Pulse Limits (PCA9685)

In `firmware/esp32_krypton/config.h`:
```cpp
#define SERVO_PULSE_MIN     102  // ~500 us (0 degrees)
#define SERVO_PULSE_MAX     512  // ~2500 us (180 degrees)
```

Different brands of MG996R servos have slight variations in internal potentiometer range. If your servo hits its internal mechanical stop before reaching 180 degrees:
- Increase `SERVO_PULSE_MIN` from `102` to `115`.
- Decrease `SERVO_PULSE_MAX` from `512` to `495`.

---

## 3. Tendon Tension Tuning

- **Under-tensioned**: Finger fails to close completely when servo reaches 180 degrees. Solution: Untie knot, pull 2-3mm tighter, re-glue.
- **Over-tensioned**: Servo buzzes continuously at 0 degrees and draws >1.5A stall current. Solution: Loosen the palmar line slightly or adjust the 0-degree angle to 5 degrees in firmware.
