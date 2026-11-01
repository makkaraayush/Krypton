# ESP8266 Servo Tester & Bench Calibration Guide

This directory contains a standalone bench testing sketch for testing your TowerPro MG996R servos with an **ESP8266** (NodeMCU, Wemos D1 Mini, or generic ESP-12), the **SZBK07 300W 20A buck converter**, and the **PCA9685 16-channel PWM driver**.

It serves as a temporary drop-in bench tester while your ESP32 is offline. It runs directly via the Arduino IDE Serial Monitor at 115200 baud without requiring Wi-Fi, WebSockets, or Python scripts.

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
| `D5` *(Optional)* | `GPIO 14` | Servo PWM Lead | Only used if testing a single servo directly without PCA9685 |

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

## 3. Pulley Testing & Calibration Sequence

1. **Check 6V Rail**: Multimeter across SZBK07 `OUT+` and `OUT-` must measure **6.00V DC**.
2. **Boot & Verify Zero (`z`)**:
   - When the sketch boots, it initializes the PCA9685 at `0x40` and moves all 5 servos to 0 degrees (open hand position).
   - If you need to re-zero at any point, enter `z`.
3. **Mount Yellow Winch Pulleys**:
   - With the servos holding firmly at 0 degrees, press each yellow `ServoPulleyHitecV5_tighter.stl` spool onto the 25T spline with its tie-off hole pointed straight toward the forearm spine.
   - Tighten the center screw.
4. **Individual Sweep (`s`)**:
   - Enter `0`, then `s` to sweep the Thumb.
   - Enter `1`, then `s` to sweep the Index.
   - Check that the braided line winds smoothly in the track without jumping off.
5. **Full Stress Test (`w`)**:
   - Enter `w` to sweep all 5 fingers simultaneously.
   - Monitor the 6.00V supply: the voltage should hold steady without resetting the ESP8266.

---

## 4. Interactive Serial Commands

| Command | Action |
| :--- | :--- |
| `0` to `4` | Selects active finger (0:Thumb, 1:Index, 2:Middle, 3:Ring, 4:Pinky) |
| `0` | Moves selected finger to 0 degrees (fully open) |
| `90` | Moves selected finger to 90 degrees (half grip) |
| `180` | Moves selected finger to 180 degrees (full fist) |
| `<any angle>` | Moves selected finger to specified degrees (0-180) |
| `all <angle>` | Moves all 5 fingers simultaneously (e.g. `all 0`, `all 90`) |
| `s` | Sweeps active finger smoothly (0 -> 180 -> 0) |
| `w` | Sweeps all 5 fingers together (buck converter stress test) |
| `z` | Zeros all 5 channels to 0 degrees (mandatory before mounting pulleys) |
| `p` | Cycles through preset poses (Open, Fist, Point, Peace, Thumbs Up, Rock On) |
| `scan` | Runs live I2C bus scanner (checks D1 and D2 lines) |
| `status` | Prints live angles and calibrated pulse limits table |
| `trim <min> <max>` | Sets custom 12-bit pulse limits for active channel (e.g. `trim 104 508`) |
| `gpio <angle>` | Drives a single servo on pin D5 directly |
| `?` or `help` | Prints command reference list |
