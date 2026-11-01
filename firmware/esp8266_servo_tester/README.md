# Standalone ESP8266 Servo Tester & Bench Calibration Guide

This directory contains the ESP8266 bench testing sketch for testing your TowerPro MG996R servos with a **12V battery**, the **SZBK07 300W 20A buck converter**, and the **PCA9685 16-channel PWM driver**.

It is the exact drop-in ESP8266 version of the `esp32_servo_tester` sketch. It does not require Wi-Fi or WebSockets; it runs directly via the Arduino IDE Serial Monitor at 115200 baud.

---

## 1. Hardware Pinout & Wiring

### Connection Diagram

```
[ 12V Battery ]
      │
      ├──(+)──> [IN+] SZBK07 300W 20A Buck [OUT+ (6.0V)] ──> [PCA9685 Green Screw V+]
      └──(–)──> [IN-] SZBK07 300W 20A Buck [OUT- (GND)]  ──> [PCA9685 Green Screw GND]

[ PCA9685 Left 6-Pin Header ]                   [ ESP8266 (NodeMCU / D1 Mini) ]
  ├── V+  (6.0V Motor Rail) ───────────────────> VIN / 5V Pin  (Powers ESP8266 onboard LDO)
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

### Connection Pinout Table

| ESP8266 Pin | NodeMCU Pin | PCA9685 Pin | Wire Type | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `VIN` / `5V` | `VIN` / `5V` | `V+` (Left Header) | Dupont | Receives 6.0V from buck to power ESP8266 onboard regulator |
| `GND` | `GND` | `GND` (Left Header) | Dupont | Common ground reference |
| `3V3` | `3V3` | `VCC` (Left Header) | Dupont | Powers PCA9685 3.3V logic (**never connect 6V to VCC**) |
| `D2` | `GPIO 4` | `SDA` (Left Header) | Dupont | I2C Serial Data line |
| `D1` | `GPIO 5` | `SCL` (Left Header) | Dupont | I2C Serial Clock line |
| `D5` *(Opt)* | `GPIO 14` | Servo Signal | Dupont | Direct pin test if testing single servo without PCA9685 |

---

## 2. Flashing Instructions (Arduino IDE)

1. **Board Configuration**:
   - Board: `NodeMCU 1.0 (ESP-12E Module)` or `LOLIN(WEMOS) D1 R2 & mini`
   - Upload Speed: `115200` or `921600`
   - CPU Frequency: `80 MHz`
   - Port: Select your ESP8266 COM port
2. **Compile & Upload**:
   - Open `firmware/esp8266_servo_tester/esp8266_servo_tester.ino`.
   - Click **Upload**.
3. **Open Serial Monitor**:
   - Set baud rate to **115200**.
   - Set line ending to **Newline** or **Both NL & CR**.
   - Press the `RST` button on your ESP8266.

---

## 3. Interactive Serial Commands

When the sketch boots, it checks for the PCA9685 at `0x40`. If found, it homes all servos to 0 degrees (open position) and displays the diagnostic menu.

| Command | Action | Purpose |
| :--- | :--- | :--- |
| `0` to `4` | Selects active channel (0:Thumb, 1:Index, 2:Middle, 3:Ring, 4:Pinky) | Switch focus finger |
| `<angle>` | Moves selected finger to specified degrees (0-180) | Fine positional testing |
| `<ch> <angle>` | Moves specific channel directly (e.g. `0 45`, `1 90`, `2 120`) | Direct single-channel move |
| `all <angle>` | Moves all 5 fingers to the same angle (e.g. `all 0`, `all 90`) | Coordinated positioning |
| `T:..,I:..` | Multi-finger packet (e.g. `T:45,I:90,M:120,R:60,P:30`) | **Varied angle test across all fingers** |
| `<5 angles>` | List of 5 varied numbers (e.g. `30 60 90 120 150`) | **Quick varied angle list** |
| `s` | Sweeps active finger smoothly (0 -> 180 -> 0) | Checks pulley winding track |
| `w` | Sweeps all 5 fingers together | **Stress test**: verifies 6.0V buck does not brownout |
| `z` | Immediately zeros all 5 channels to 0 degrees | **Mandatory before mounting pulleys** |
| `p` | Cycles through preset poses (Open, Fist, Point, Peace, Rock On) | Quick posture checks |
| `scan` | Runs live I2C bus scanner | Verifies SDA (D2) and SCL (D1) wiring |
| `status` | Prints live angles and calibrated pulse limits table | Verification diagnostic |
| `trim <min> <max>` | Sets custom 12-bit pulse limits (e.g. `trim 104 508`) | Stops resting servo hum |
| `gpio <angle>` | Drives a single servo on ESP8266 pin D5 directly | Test without PCA9685 |
| `?` or `help` | Prints command reference list | Cheat sheet |

---

## 4. Pulley Bench Testing Sequence

1. **Multimeter Check**: Verify `OUT+` on the SZBK07 buck converter measures **6.00V DC**.
2. **Flash & Zero (`z`)**: Run `z`. All 5 servos lock at 0 degrees.
3. **Mount Yellow Pulleys**: Press the yellow `ServoPulleyHitecV5_tighter.stl` winches onto the 25T splines with the line holes pointing straight down the forearm axis. Fasten center screws.
4. **Varied Angle Check**:
   - Send `T:45,I:90,M:120,R:60,P:30` or `30 60 90 120 150`.
   - Inspect each finger's tendon tension and pulley spool winding at different flexions.
5. **Sweep Test (`s` & `w`)**:
   - Run `s` on each finger to check for joint binding.
   - Run `w` to sweep all 5 fingers at once and ensure the 6V rail stays steady.
