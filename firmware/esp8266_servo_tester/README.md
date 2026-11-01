# ESP8266 Complete Servo Controller & Bench Tester

This sketch is a drop-in replacement for the ESP32 controller, engineered specifically for the **ESP8266** (NodeMCU, Wemos D1 Mini, or generic ESP-12) to test pulleys, check varied multi-finger angles, and tune tendons via the **SZBK07 300W 20A buck converter** and the **PCA9685 16-channel PWM driver**.

It matches the ESP32 firmware features:
- **Same Input Commands**: Supports the standard telemetry packet (`T:45,I:90,M:120,R:60,P:30`), direct 5-number varied lists (`30 60 90 120 150`), and `POSE:...` commands.
- **Varied Multi-Finger Angles**: Test completely different angles on each finger simultaneously to evaluate grip combinations and tendon tension.
- **Trajectory Smoothing**: Includes the same software motion interpolation (~66Hz easing) to step servos gradually and avoid snapping braided lines under sudden torque spikes.
- **Cyberpunk Web Dashboard**: Broadcasts `Krypton-Arm` Wi-Fi network with an embedded mobile-responsive dark dashboard (`http://192.168.4.1`) featuring 5 independent finger sliders and preset buttons.
- **UDP Telemetry (Port 8888)**: Compatible with `tools/servo_test.py` and the Python vision tracking pipeline.

---

## 1. Hardware Pinout & Wiring

### Connection Diagram

```
[ 12V Battery ]
      │
      ├──(+)──> [IN+] SZBK07 300W 20A Buck [OUT+ (6.0V)] ──> [PCA9685 Green Screw V+]
      └──(–)──> [IN-] SZBK07 300W 20A Buck [OUT- (GND)]  ──> [PCA9685 Green Screw GND]

[ PCA9685 Left 6-Pin Header ]                   [ ESP8266 (NodeMCU / D1 Mini) ]
  ├── V+  (6.0V Motor Rail) ───────────────────> VIN / 5V Pin (Powers onboard 3.3V LDO)
  ├── GND (Common Ground)   ───────────────────> GND Pin
  ├── VCC (3.3V Logic Rail) <─────────────────── 3V3 Pin (Powers PCA9685 chip)
  ├── SDA (I2C Data)        <─────────────────── D2 Pin (GPIO 4)
  ├── SCL (I2C Clock)       <─────────────────── D1 Pin (GPIO 5)
  └── OE  (Output Enable)   (Leave Disconnected)

[ PCA9685 3-Pin Servo Headers (Channels 0 to 4) ]
  ├── Channel 0 ──> Thumb MG996R Servo
  ├── Channel 1 ──> Index MG996R Servo
  ├── Channel 2 ──> Middle (Majeure) MG996R Servo
  ├── Channel 3 ──> Ring MG996R Servo
  └── Channel 4 ──> Pinky (Auriculaire) MG996R Servo
```

### Pin Table

| ESP8266 Pin | NodeMCU GPIO | PCA9685 Pin | Notes |
| :--- | :--- | :--- | :--- |
| `VIN` / `5V` | Internal LDO | `V+` (Left Header) | Receives 6.0V from buck converter to power board |
| `GND` | Ground | `GND` | Common ground reference |
| `3V3` | 3.3V Rail | `VCC` | Powers PCA9685 logic. **Never connect 6.0V to VCC!** |
| `D2` | `GPIO 4` | `SDA` | I2C Data line (400kHz fast mode) |
| `D1` | `GPIO 5` | `SCL` | I2C Clock line (400kHz fast mode) |

---

## 2. Flashing Instructions (Arduino IDE)

1. **Install ESP8266 Core**:
   - Go to *File* -> *Preferences*.
   - Add `http://arduino.esp8266.com/stable/package_esp8266com_index.json` to Additional Board Manager URLs.
   - Go to *Tools* -> *Board* -> *Boards Manager*, search for `esp8266`, and install it.
2. **Select Board**:
   - For NodeMCU: Select `NodeMCU 1.0 (ESP-12E Module)`.
   - For Wemos: Select `LOLIN(WEMOS) D1 R2 & mini`.
   - Upload Speed: `115200` or `921600`.
   - CPU Frequency: `80 MHz`.
3. **Open and Upload**:
   - Open `esp8266_servo_tester.ino`.
   - Select your COM port and click **Upload**.
   - Open Serial Monitor at **115200 baud** (set line ending to **Newline**).

---

## 3. Controlling Varied Angles

You can test varied multi-finger positions using any of the following input methods:

### Method A: ESP32 Telemetry String (Serial or UDP)
Send exact angle assignments in one line:
```text
T:45,I:90,M:120,R:60,P:30
```
*(Sets Thumb to 45°, Index to 90°, Middle to 120°, Ring to 60°, Pinky to 30°)*

You can also send partial updates:
```text
T:0,I:180
```

### Method B: Shorthand Number Lists
Type 5 numbers separated by spaces or commas:
```text
30 60 90 120 150
```
or
```text
angles 10, 45, 90, 135, 180
```

### Method C: Channel + Angle Direct Pairs
```text
0 45      (Sets Channel 0 / Thumb to 45 deg)
1 90      (Sets Channel 1 / Index to 90 deg)
2 120     (Sets Channel 2 / Middle to 120 deg)
3 60      (Sets Channel 3 / Ring to 60 deg)
4 180     (Sets Channel 4 / Pinky to 180 deg)
```

### Method D: Web Dashboard Sliders (Wireless Phone / PC)
1. Connect to Wi-Fi access point: **`Krypton-Arm`** (Password: **`cyberpunk2026`**).
2. Open a browser and go to **`http://192.168.4.1`**.
3. Drag the individual finger sliders to inspect pulley wrapping at varied angles in real time.

### Method E: Python Test Utility
From the `tools/` folder on your computer:
```bash
# Over Serial:
python tools/servo_test.py --mode serial --channel 1 --angle 90

# Preset Poses:
python tools/servo_test.py --mode serial --pose rock
```

---

## 4. Bench Testing & Calibration Commands

| Command | Action | Purpose |
| :--- | :--- | :--- |
| `z` | Immediately zeros all 5 channels to 0° | **Mandatory before mounting pulleys** |
| `s` | Sweeps active finger (0 -> 180 -> 0) | Checks pulley winding track |
| `w` | Sweeps all 5 fingers simultaneously | High-current stress test for buck converter |
| `smooth` | Toggles trajectory easing ON / OFF | Switch between smooth easing and instant jump |
| `all <angle>` | Moves all fingers to the same angle | Uniform closure test (e.g. `all 90`) |
| `fist`, `open`, `peace`, `point`, `rock_on`, `pinch` | Applies preset gestures | Quick posture checks |
| `trim <min> <max>` | Sets calibrated 12-bit pulse limits | Stops resting potentiometer buzz |
| `status` | Prints live angles and pulse trims table | Verifies internal state |
| `scan` | Runs live I2C bus scanner | Verifies SDA/SCL lines |
| `?` or `help` | Prints command reference list | Cheat sheet |
