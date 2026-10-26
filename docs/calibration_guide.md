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

## 2. Software Pulse Limits & Per-Finger Trims

In `firmware/esp32_krypton/config.h`:
```cpp
// Per-finger pulse trims (12-bit PCA9685 ticks, 0 to 4095 @ 50Hz)
const uint16_t SERVO_PULSE_MIN_MAP[NUM_SERVOS] = { 102, 105, 102, 104, 106 }; // Thumb, Index, Middle, Ring, Pinky
const uint16_t SERVO_PULSE_MAX_MAP[NUM_SERVOS] = { 510, 512, 512, 508, 505 };
```

Different production batches of TowerPro MG996R servos have slight internal potentiometer variations. Rather than un-screwing the winch spool and jumping teeth on the 25T spline, you can fine-tune mechanical end-stops in firmware:

1. **Test Channel Centering**:
   Run the CLI test tool to command a single finger to 0 degrees:
   ```bash
   python tools/servo_test.py --mode udp --channel 1 --angle 0
   ```
2. **Listen for Potentiometer Jitter**:
   If the servo motor buzzes or hums at 0 degrees, it is being mechanically forced against its internal physical stop. Increase `SERVO_PULSE_MIN_MAP` for that channel by 3 to 5 ticks (e.g. from `102` to `106`).
3. **Verify Full Contraction**:
   Command the finger to 180 degrees:
   ```bash
   python tools/servo_test.py --mode udp --channel 1 --angle 180
   ```
   If the finger stops short of a complete fist, increase `SERVO_PULSE_MAX_MAP` slightly until full grip closure is achieved.

---

## 3. Tendon Tension Tuning

- **Under-tensioned**: Finger fails to close completely when servo reaches 180 degrees. Solution: Untie knot, pull 2-3mm tighter, re-glue.
- **Over-tensioned**: Servo buzzes continuously at 0 degrees and draws >1.5A stall current. Solution: Loosen the palmar line slightly or adjust the 0-degree angle to 5 degrees in firmware.
