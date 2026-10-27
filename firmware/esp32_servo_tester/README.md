# Standalone ESP32 Servo Tester & Bench Calibration Guide

This directory contains an interactive ESP32 bench testing sketch designed specifically for testing your TowerPro MG996R servos with a **12V battery**, the **SZBK07 300W 20A buck converter**, and the **PCA9685 16-channel PWM driver**.

It does not require Wi-Fi, WebSockets, or OpenCV. It runs directly via the Arduino IDE Serial Monitor at 115200 baud.

---

## 1. Hardware Overview

- **Power Source**: 12V Battery (3S Li-ion 11.1V-12.6V or 12V Lead-Acid / LiFePO4).
- **Voltage Regulator**: SZBK07 (300W 20A Constant-Current / Constant-Voltage Synchronous Step-Down Buck Converter).
- **Controller**: ESP32 Type-C (38-pin or 30-pin CP2102).
- **PWM Driver**: PCA9685 16-Channel 12-bit I2C driver (Address `0x40`).
- **Servos**: 5x TowerPro MG996R metal-gear servos (Channels 0 to 4).

---

## 2. Crucial Pre-Power SZBK07 Calibration (DO THIS FIRST)

> **CRITICAL WARNING**: The SZBK07 buck converter arrives from the manufacturer with its trimmer screws set at unknown positions. If you hook your 12V battery up and plug directly into your PCA9685 without calibrating, it could deliver 11V to 12V directly to your 6V servos and burn out their control boards instantly.

Follow these 4 calibration steps before plugging anything into the converter output:

1. **Disconnect All Loads**:
   Keep the PCA9685, ESP32, and all servos completely disconnected from the buck converter output.
2. **Connect 12V Battery**:
   Connect your 12V battery positive lead to `IN+` and negative lead to `IN-` on the SZBK07 terminal blocks. The onboard power LED will light up.
3. **Calibrate Voltage (CV Trimmer)**:
   - Set your digital multimeter to DC Volts ($20\text{V}$ range).
   - Place the black multimeter probe on `OUT-` and the red probe on `OUT+`.
   - Locate the **CV (Constant Voltage)** brass trimmer screw (located next to the output terminals).
   - Using a small flathead screwdriver, turn the CV trimmer counter-clockwise until the multimeter reads **exactly 6.00V DC** (acceptable range: $5.95\text{V}$ to $6.05\text{V}$).
4. **Open Current Throttle (CC Trimmer)**:
   - Locate the **CC (Constant Current)** brass trimmer screw (located toward the middle of the board).
   - Turn the CC trimmer screw **12 to 15 full revolutions clockwise**.
   - This opens up the current path to its full 20A limit so the converter will not choke or drop voltage when multiple servos pull peak stall current together.

---

## 3. Wiring Diagram

```
[ 12V Battery ]
      │
      ├──(+)──> [IN+]  SZBK07 300W 20A Buck  [OUT+ (6.0V)] ──┬──> [PCA9685 V+ Green Terminal]
      │                                                       └──> [ESP32 VIN Pin]
      └──(–)──> [IN-]  SZBK07 300W 20A Buck  [OUT- (GND)]  ──┬──> [PCA9685 GND Green Terminal]
                                                              └──> [ESP32 GND Pin]

[ ESP32 ]                                    [ PCA9685 Logic Header (6-Pin Side) ]
  ├── 3.3V Pin ────────────────────────────> VCC (Logic Power: 3.3V ONLY)
  ├── GND Pin  ────────────────────────────> GND (Logic Ground)
  ├── GPIO 21 (SDA) ───────────────────────> SDA (I2C Data)
  └── GPIO 22 (SCL) ───────────────────────> SCL (I2C Clock)

[ PCA9685 3-Pin Servo Headers (Channels 0 to 4) ]
  ├── Channel 0 ──> Thumb MG996R Servo
  ├── Channel 1 ──> Index MG996R Servo
  ├── Channel 2 ──> Middle (Majeure) MG996R Servo
  ├── Channel 3 ──> Ring MG996R Servo
  └── Channel 4 ──> Pinky (Auriculaire) MG996R Servo
```

---

## 4. Connection Pinout Tables

### A. Power Connections (From Calibrated SZBK07 Buck)

| SZBK07 Terminal | Target Destination | Wire Gauge | Notes |
| :--- | :--- | :--- | :--- |
| `OUT+ (6.0V)` | `PCA9685 V+` (Green screw terminal) | 18 AWG / 20 AWG | High-current servo motor power rail |
| `OUT+ (6.0V)` | `ESP32 VIN` pin | 22 AWG / Dupont | ESP32 onboard LDO drops 6.0V to 3.3V cleanly |
| `OUT- (GND)` | `PCA9685 GND` (Green screw terminal) | 18 AWG / 20 AWG | High-current ground return |
| `OUT- (GND)` | `ESP32 GND` pin | 22 AWG / Dupont | Common reference ground |

### B. Logic Connections (ESP32 to PCA9685)

| ESP32 Pin | PCA9685 Pin | Wire Color | Notes |
| :--- | :--- | :--- | :--- |
| `3.3V` | `VCC` | Red Dupont | **Never connect to 6V!** PCA9685 logic must match ESP32 3.3V rail. |
| `GND` | `GND` | Black Dupont | Common logic ground. |
| `GPIO 21` | `SDA` | Green / Blue | I2C Serial Data line. |
| `GPIO 22` | `SCL` | Yellow / White | I2C Serial Clock line. |
| *(None)* | `OE` | Leave Disconnected | Output Enable is active-low with internal pull-down. |

### C. MG996R Servo 3-Pin Lead Color Code

| Servo Wire Color | Header Row on PCA9685 | Function |
| :--- | :---: | :--- |
| **Brown** | Bottom Row (`GND` / Black) | Ground |
| **Red** | Middle Row (`V+` / Red) | +6.0V Motor Power |
| **Orange / Yellow** | Top Row (`PWM` / Yellow) | 50Hz Control Signal |

---

## 5. How to Flash & Run the Test Suite

1. **Open Sketch**:
   Open `firmware/esp32_servo_tester/esp32_servo_tester.ino` in Arduino IDE.
2. **Board Configuration**:
   - Board: `ESP32 Dev Module`
   - Upload Speed: `921600`
   - Port: Select your ESP32 COM port (e.g. `COM3` on Windows).
3. **Upload**:
   Click **Upload**. No third-party libraries need to be installed; it uses the built-in `Wire.h` library.
4. **Open Serial Monitor**:
   - Set baud rate to **115200**.
   - Set line ending to **Newline** or **Both NL & CR**.
   - Press the `EN` / `RST` button on your ESP32.

---

## 6. Interactive Serial Commands

When the sketch boots, it will automatically check for the PCA9685 at address `0x40`. If found, it homes all servos to 0 degrees and prints the diagnostic menu.

| Command | Action | Purpose |
| :--- | :--- | :--- |
| `0` to `4` | Selects active channel (0:Thumb, 1:Index, 2:Middle, 3:Ring, 4:Pinky) | Switch focus finger |
| `0` | Moves selected finger to 0 degrees | Fully open / relaxed |
| `90` | Moves selected finger to 90 degrees | Halfway grip |
| `180` | Moves selected finger to 180 degrees | Full contracted fist |
| `<any angle>` | Moves selected finger to specified degrees (0-180) | Fine positional testing |
| `all <angle>` | Moves all 5 fingers simultaneously (e.g. `all 0`, `all 90`) | Coordinated positioning |
| `s` | Sweeps active finger smoothly (0 -> 180 -> 0) | Checks for joint friction / binding |
| `w` | Sweeps all 5 fingers together | **Stress test**: verifies SZBK07 does not brownout |
| `z` | Immediately zeros all 5 channels to 0 degrees | **Mandatory before mounting pulleys** |
| `p` | Cycles through preset poses (Open, Fist, Point, Peace, Rock) | Demonstrates full hand articulation |
| `scan` | Runs live I2C bus scanner | Verifies SDA/SCL wiring if board isn't found |
| `status` | Prints live angles and calibrated pulse limits table | Verification diagnostic |
| `trim <min> <max>` | Sets custom 12-bit pulse limits (e.g. `trim 104 508`) | Stops resting servo hum |
| `gpio <angle>` | Drives a single servo on ESP32 GPIO 18 directly | Test without PCA9685 attached |
| `?` or `help` | Prints command reference list | Cheat sheet |

---

## 7. Recommended Bench Testing Sequence

1. **Multimeter Check**: Verify `OUT+` on the SZBK07 is measuring 6.00V DC.
2. **Flash the Sketch**: Upload and open the Serial Monitor at 115200 baud.
3. **Verify Zeroing (`z`)**: Run `z`. All 5 servo shafts will rotate to their 0-degree open position.
4. **Mount Pulleys**: With power active and servos holding at 0 degrees, press the yellow `ServoPulleyHitecV5_tighter.stl` winches onto the 25T splines with the string tie holes aligned straight toward the forearm spine. Tighten the center screw.
5. **Individual Sweep (`0`, then `s`, `1`, then `s`...)**: Test each finger one by one. Check that the braided line winds cleanly around the pulley track without jumping off.
6. **Stress Test (`w`)**: Sweep all 5 fingers together. Place your multimeter across `OUT+` and `OUT-` on the SZBK07. The voltage should remain between $5.90\text{V}$ and $6.05\text{V}$ without resetting the ESP32.
