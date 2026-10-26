# Krypton Hardware & Power

This directory covers everything related to physical hardware, electronic components, wiring schematics, and power delivery for the Krypton bionic arm.

## Hardware Guides

- [bom.md](bom.md): Full mechanical and electronic bill of materials. Details fastener counts for the complete forearm spine (`robpart1V2` through `robpart5V4`), `RobServoBedV5`, and M3x20 knuckle axles.
- [wiring.md](wiring.md): Complete electrical schematic, pinouts, buck converter calibration, and wire harness strain relief guidelines.
- [power_guide.md](power_guide.md): Details on 3S Li-ion battery discharge rates, stall current calculations (up to 12.5A peak), and why standard buck converters fail.

## Core Component Summary

| Component | Part / Spec | Role |
| :--- | :--- | :--- |
| Microcontroller | ESP32 Type-C (38-Pin CP2102) | Brain: Dual-core 240MHz, Wi-Fi web server, UDP socket, trajectory interpolation |
| PWM Driver | PCA9685 16-Channel 12-Bit | Jitter-free hardware PWM pulses over I2C, isolates 3.3V logic from 6.0V power |
| Actuators | 5x TowerPro MG996R (180-deg) | High-torque metal-gear digital servos (11 kg-cm @ 6.0V) |
| Power Regulation | 300W 20A DC-DC Buck Converter | Drops 3S battery (11.1V - 12.6V) to steady 6.0V without brownout |
| Battery | 3S 18650 Pack (11.1V - 12.6V) | High-current power source |
| Tendon Line | 110 lb / 0.60mm PE Braided Line | Zero-stretch, high-abrasion finger actuation cables |
