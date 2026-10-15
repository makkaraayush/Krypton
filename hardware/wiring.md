# Electrical Wiring & Calibration Guide

This guide details the physical wiring and the critical buck converter calibration procedure.

## Pre-Power Buck Converter Calibration (DO THIS FIRST!)

> **WARNING**: The 300W 20A buck converter ships from the factory with its trimmer potentiometers in random positions. If you connect it directly to your PCA9685 and ESP32 without calibration, it could output 12V and destroy your servos and boards!

1. **Disconnect Everything**: Ensure no servos, ESP32, or PCA9685 boards are connected to the buck converter output.
2. **Connect Battery**: Attach your 3S 18650 battery pack (11.1V - 12.6V) to `IN+` and `IN-` on the buck converter.
3. **Calibrate Voltage (CV)**:
   - Set your digital multimeter to DC Voltage measurement.
   - Place multimeter probes across `OUT+` and `OUT-`.
   - Using a small flathead screwdriver, turn the **CV (Constant Voltage)** brass trim pot counter-clockwise until the output reads **exactly 6.00V DC**.
4. **Calibrate Current (CC)**:
   - Turn the **CC (Constant Current)** brass trim pot **12 to 15 full turns clockwise**.
   - This opens up the current delivery path completely so the converter never throttles under high grip loads when all 5 servos pull peak current.

---

## System Connection Diagram

```
[ 3S 18650 Battery (11.1V - 12.6V) ]
       │
       ├──(+)──> [IN+] 300W 20A Buck Converter [OUT+ (6.0V)] ──┬──> [PCA9685 Green Terminal V+]
       │                                                       └──> [ESP32 VIN Pin]
       └──(–)──> [IN-] 300W 20A Buck Converter [OUT- (GND)]  ──┬──> [PCA9685 Green Terminal GND]
                                                               └──> [ESP32 GND Pin]

[ ESP32 38-Pin ]                       [ PCA9685 Logic Header ]
  ├── 3.3V Pin ──────────────────────> VCC (Logic Power)
  ├── GND Pin  ──────────────────────> GND (Logic Ground)
  ├── GPIO 21 (SDA) ─────────────────> SDA (I2C Data)
  └── GPIO 22 (SCL) ─────────────────> SCL (I2C Clock)

[ PCA9685 Servo Headers (Channels 0 to 4) ]
  ├── Channel 0 ──> Thumb MG996R Servo
  ├── Channel 1 ──> Index MG996R Servo
  ├── Channel 2 ──> Middle (Majeure) MG996R Servo
  ├── Channel 3 ──> Ring MG996R Servo
  └── Channel 4 ──> Pinky (Auriculaire) MG996R Servo
```

---

## Pinout Table

### ESP32 to PCA9685

| ESP32 Pin | PCA9685 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| `3.3V` | `VCC` | Logic Power | **Do not** connect to 5V/6V! PCA9685 logic must match ESP32 3.3V logic. |
| `GND` | `GND` | Common Ground | Essential for reliable I2C signaling. |
| `GPIO 21` | `SDA` | I2C Serial Data | 400kHz fast mode supported. |
| `GPIO 22` | `SCL` | I2C Serial Clock | 400kHz fast mode supported. |

### Buck Converter to Boards

| Buck Output | Destination | Notes |
| :--- | :--- | :--- |
| `OUT+ (6.0V)` | `PCA9685 V+` (Green Screw Terminal) | Powers servo motor rails. Use 18/20 AWG wire. |
| `OUT+ (6.0V)` | `ESP32 VIN` | Onboard ESP32 LDO steps 6.0V down to 3.3V cleanly. |
| `OUT- (GND)` | `PCA9685 GND` (Green Screw Terminal) | High-current ground return. |
| `OUT- (GND)` | `ESP32 GND` | Common ground reference. |

---

## PCA9685 Servo Channel Assignment

| Channel | Finger | Mechanical Joint | Motion Direction |
| :---: | :--- | :--- | :--- |
| `0` | Thumb | CMC / MCP / IP Flexion | Spool pulls palmar tendon |
| `1` | Index | MCP / PIP / DIP Flexion | Spool pulls palmar tendon |
| `2` | Middle | MCP / PIP / DIP Flexion | Spool pulls palmar tendon |
| `3` | Ring | MCP / PIP / DIP Flexion | Spool pulls palmar tendon |
| `4` | Pinky | MCP / PIP / DIP Flexion | Spool pulls palmar tendon |
