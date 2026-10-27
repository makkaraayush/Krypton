# Krypton - 5-Finger Cyberpunk Bionic Arm

Krypton is a 5-degree-of-freedom tendon-actuated robotic bionic arm. It pairs 3D-printed articulation with an ESP32 microcontroller, a 16-channel PCA9685 PWM driver, high-torque MG996R metal-gear servos, an onboard web control interface, and real-time computer vision teleoperation using OpenCV and MediaPipe.

The mechanical joint foundation is inspired by the open-source InMoov robot arm, but redesigned with custom cyberpunk armor plating, M3 socket-head axle pins (replacing loose dowel pins), an optimized 5-spool multi-color print workflow on a Bambu Lab A1 + AMS Lite, and modernized electronics capable of wireless browser and vision control.

---

## Architecture Overview

```
[ Human User / Webcam ]
         │
         ▼  (MediaPipe 21-Landmark Tracking)
[ Python Vision Pipeline (OpenCV) ]
         │
         ├─── USB Serial (115200 Baud) ──┐
         └─── Wi-Fi UDP (Port 8888) ─────┤
                                         ▼
                               [ ESP32 Controller ] <─── Wi-Fi Web Dashboard (Port 80/81)
                                         │
                                         ▼  (I2C Fast Mode @ 400kHz)
                               [ PCA9685 16-Ch PWM ]
                                         │
            ┌──────────────┬─────────────┼─────────────┬──────────────┐
            ▼              ▼             ▼             ▼              ▼
       [Thumb Servo] [Index Servo] [Middle Servo] [Ring Servo]  [Pinky Servo]
            │              │             │             │              │
      (110 lb PE)    (110 lb PE)   (110 lb PE)   (110 lb PE)    (110 lb PE)
            ▼              ▼             ▼             ▼              ▼
       [Thumb Joint]  [Index Finger] [Middle Joint] [Ring Finger]  [Pinky Finger]
```

---

## Key Specifications

- **Actuation**: 5x TowerPro MG996R digital metal-gear servos (180-degree positional, 11 kg-cm torque).
- **Controller**: ESP32 Type-C CP2102 (Dual-core 240MHz, 2.4GHz Wi-Fi + BLE).
- **PWM Offload**: PCA9685 16-Channel 12-bit I2C driver (address `0x40`, 50Hz frequency).
- **Power System**: 3S 18650 Li-ion battery pack (11.1V - 12.6V) stepped down to 6.0V via a 300W 20A constant-current/constant-voltage buck converter.
- **Tendon Rigging**: 110 lb (50 kg) / 0.60mm 4-strand PE braided fishing line.
- **Pivot Axles**: High-tensile M3 x 20mm black-oxide socket-head screws paired with SS304 M3 Nyloc locknuts.
- **Teleoperation**: Real-time right-hand skeletal tracking via OpenCV + MediaPipe streaming over UDP (<10ms latency) or USB Serial.
- **Web Interface**: Lightweight cyberpunk dark dashboard hosted directly in ESP32 PROGMEM with WebSocket controls and preset poses.

---

## Directory Structure

- [hardware/](hardware/README.md)
  - [bom.md](hardware/bom.md): Complete mechanical, fastener, and electronics bill of materials with cost optimizations.
  - [wiring.md](hardware/wiring.md): Electrical schematic, buck converter calibration guide, wire harness routing, and pinout tables.
  - [power_guide.md](hardware/power_guide.md): 3S battery sizing, stall current spikes, and voltage safety.
- [models/](models/README.md)
  - [filament_palette.md](models/filament_palette.md): 5-spool cyberpunk colorway, assigned STLs, and AMS purge volumes.
  - [plates_and_slicing.md](models/plates_and_slicing.md): Multi-plate zero-waste consolidation plan, Gyroid infill, and modifier setups.
  - [stl/](models/stl/): Organized 3D print assets for fingers, forearm spine (`robpart1V2` to `robpart5V4`), wrist rotation mechanism, servo drive bed, and cyber armor.
  - [3mf/](models/3mf/): Sliced Bambu Studio project files for plate consolidation and single-finger orientation.
- [firmware/](firmware/README.md)
  - [esp32_krypton/](firmware/esp32_krypton/): Production firmware with async web dashboard, WebSockets, UDP socket receiver, and trajectory smoothing.
  - [esp32_servo_tester/](firmware/esp32_servo_tester/): Standalone bench testing sketch and wiring guide for 12V battery + SZBK07 buck converter + PCA9685.
- [vision/](vision/README.md)
  - [hand_tracker.py](vision/hand_tracker.py): Real-time MediaPipe right-hand tracking loop with multi-knuckle scale normalization.
  - [communicator.py](vision/communicator.py): Multi-mode transmission handler (Serial with auto-reconnect, Wi-Fi UDP).
  - [visualizer.py](vision/visualizer.py): Cyberpunk HUD overlay with live tension progress bars and keybinding help modal.
- [tools/](tools/)
  - [servo_test.py](tools/servo_test.py): Standalone CLI utility for sweeping and testing individual servo channels over Serial or UDP.
- [docs/](docs/README.md)
  - [assembly_guide.md](docs/assembly_guide.md): Step-by-step mechanical assembly, hole reaming, servo bed installation, and tendon stringing.
  - [calibration_guide.md](docs/calibration_guide.md): Electrical zeroing, per-finger pulse trimming, and tension tuning.
  - [troubleshooting.md](docs/troubleshooting.md): Common failure modes, knot slippage prevention, jitter fixes, and brownout solutions.

---

## Quick Start

### 1. 3D Printing
Print Plates 1, 2, and 3 as single-color batches (Black, Dark Grey, Yellow). Print Plate 4 on your Bambu Lab A1 + AMS Lite for the multi-color cyber armor. See [models/plates_and_slicing.md](models/plates_and_slicing.md).

### 2. Electronics & Wiring
Before connecting the ESP32 or servos, connect your 3S battery to the 300W buck converter and adjust the CV trim pot to **6.0V DC**. Open the CC trim pot 12-15 turns clockwise. Follow [hardware/wiring.md](hardware/wiring.md).

### 3. Firmware Flashing
Open `firmware/esp32_krypton/esp32_krypton.ino` in Arduino IDE or VS Code with PlatformIO. Flash to the ESP32. It will broadcast an access point named `Krypton-Arm` (password: `cyberpunk2026`). Connect from your phone and navigate to `http://192.168.4.1` to test finger sliders.

### 4. Vision Teleoperation
Install Python dependencies and run the hand tracker:
```bash
cd vision
pip install -r requirements.txt
python hand_tracker.py --mirror --mode udp --ip 192.168.4.1
```
Aim your webcam at your right hand. The arm will mirror your finger movements in real time.

---

## License

MIT License. See [LICENSE](LICENSE) for details.
