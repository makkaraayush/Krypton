# Krypton Documentation Index

This directory contains the assembly, calibration, and maintenance documentation for Krypton.

## Core Guides

- [assembly_guide.md](assembly_guide.md): Step-by-step mechanical assembly guide covering hole reaming, M3 knuckle pinning, `RobServoBedV5` installation, tendon cable comb routing, and stringing.
- [calibration_guide.md](calibration_guide.md): Electrical calibration, buck converter voltage tuning, per-finger pulse trimming (`SERVO_PULSE_MIN_MAP`), and software tension adjustments.
- [troubleshooting.md](troubleshooting.md): Diagnosing servo jitter, knuckle binding, knot slippage prevention, brownout resets, and PCA9685 I2C errors.

## Related System Docs

- [Hardware & BOM](../hardware/README.md): Component sourcing, fastener specs, and high-current wiring.
- [3D Printing Hub](../models/README.md): Plate schedules, Bambu A1 + AMS Lite settings, and filament palette.
- [Firmware Architecture](../firmware/README.md): Web dashboard, UDP telemetry receiver, and trajectory easing engine.
- [Vision Teleoperation](../vision/README.md): MediaPipe hand tracking, 3D kinematic math, and communication modes.
