# Krypton ESP32 Firmware

The firmware runs on an ESP32 dual-core microcontroller and offloads 50Hz PWM signal generation to a PCA9685 16-channel 12-bit I2C driver.

## Firmware Packages

- [esp32_krypton/](esp32_krypton/): Production firmware with async cyberpunk web dashboard, WebSockets, UDP socket receiver, and gesture engine.
- [esp32_servo_tester/](esp32_servo_tester/): Standalone bench testing and calibration sketch for 12V battery + SZBK07 buck converter + PCA9685. Features interactive serial menu, zeroing mode, single/multi-finger sweep, and live pulse trimming.

---

## Features (Production Firmware)

- **Asynchronous Web Dashboard**: Cyberpunk-themed dark interface served from ESP32 PROGMEM.
- **WebSocket Channel (Port 81)**: Sub-50ms latency slider controls from smartphones or desktop browsers.
- **UDP Telemetry Socket (Port 8888)**: High-speed packet receiver for real-time OpenCV/MediaPipe hand tracking.
- **Serial USB Parser (115200 Baud)**: Wired teleoperation fallback.
- **Trajectory Interpolator**: Software motion smoothing engine that ramps servo speeds and prevents instantaneous torque jerks that can snap braided line tendons.

---

## Flashing Instructions

### Using Arduino IDE

1. **Install ESP32 Board Package**:
   - In Arduino IDE, go to *File* -> *Preferences*.
   - Add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to Additional Board Manager URLs.
   - Go to *Boards Manager*, search for `esp32`, and install version `2.0.14` or later.
2. **Install Required Libraries**:
   - `WebSockets` by Markus Sattler (Search `WebSocketsServer` in Library Manager).
   - `Wire` (built-in).
   - `WiFi` (built-in).
   - `WiFiUdp` (built-in).
3. **Select Board**:
   - Board: `ESP32 Dev Module`
   - Upload Speed: `921600`
   - CPU Frequency: `240MHz`
   - Flash Frequency: `80MHz`
   - Partition Scheme: `Default 4MB with spiffs`
4. **Compile & Upload**:
   - Open `firmware/esp32_krypton/esp32_krypton.ino`.
   - Connect the ESP32 via Type-C USB and click **Upload**.

---

## Network Connection

1. On boot, the ESP32 broadcasts a Wi-Fi Access Point:
   - **SSID**: `Krypton-Arm`
   - **Password**: `cyberpunk2026`
   - **Gateway IP**: `192.168.4.1`
2. Connect your smartphone or laptop to `Krypton-Arm`.
3. Open a browser and navigate to `http://192.168.4.1`.

---

## Protocol Reference

### REST Endpoints

- `GET /`: Serves the responsive web dashboard.
- `GET /api/status`: Returns current and target angles as JSON:
  ```json
  { "angles": [0.0, 0.0, 0.0, 0.0, 0.0], "target": [0.0, 0.0, 0.0, 0.0, 0.0] }
  ```
- `POST /api/servos`: Sets individual finger angle:
  ```json
  { "channel": 1, "angle": 90.0 }
  ```
- `POST /api/pose`: Triggers a preset gesture:
  ```json
  { "name": "FIST" }
  ```

### UDP Telemetry Packet (Port 8888)

Compact ASCII string sent from Python vision tracking:
```
T:45.0,I:90.0,M:120.0,R:120.0,P:120.0

```

### Serial USB Commands (115200 Baud)

- Set angles: `T:90,I:120,M:120,R:120,P:120
`
- Trigger pose: `POSE:PEACE
`
