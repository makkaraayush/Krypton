# Krypton Computer Vision Teleoperation

This module tracks your hand in real time via webcam using MediaPipe and OpenCV, translates finger flexion into joint angles, and streams them wirelessly (or via USB Serial) to the ESP32.

## Requirements

- Python 3.10+
- Webcam (720p or 1080p recommended)
- Dependencies in `requirements.txt`:
  ```bash
  pip install -r requirements.txt
  ```

---

## Running the Tracker

### Quick Launch (Windows)
Double-click `run_tracker.bat`.

### Command Line
```bash
python hand_tracker.py --mirror --mode udp --ip 192.168.4.1
```

### Available Arguments

| Flag | Default | Description |
| :--- | :---: | :--- |
| `--cam` | `0` | Camera device index (use `1` or `2` for external USB webcam) |
| `--mode` | `udp` | Telemetry mode: `udp`, `serial`, or `none` |
| `--ip` | `192.168.4.1` | ESP32 IP address when using UDP |
| `--port` | Auto-detect | Serial COM port (e.g. `COM3` on Windows, `/dev/ttyUSB0` on Linux) |
| `--mirror` | `True` | Horizontally flips the camera view so mirroring feels natural |

---

## Keyboard Controls

- `q`: Quit the program.
- `m`: Cycle transmission mode (`UDP` -> `Serial` -> `None`).
- `c`: Recalibrate / tare zero point (resets all fingers to flat 0 degrees).
