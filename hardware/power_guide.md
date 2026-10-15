# Power Supply & Battery Guidelines

## Why a 300W 20A Buck Converter is Necessary

Standard hobby robotics tutorials often recommend cheap LM2596 buck converters or powering servos from an Arduino/ESP32 5V pin. In a 5-finger bionic arm with metal-gear servos, this causes immediate failure:

- **Idle Current**: ~80 mA to 100 mA per servo (~500 mA total).
- **Moving Current**: ~500 mA to 800 mA per servo (~2.5 A to 4 A total).
- **Stall Current**: ~2.5 A per servo. If all 5 fingers grasp an object firmly, total instantaneous current can reach **10.0 A to 12.5 A**.

A standard LM2596 module is rated for 2A continuous (3A peak with heatsink). When two or three fingers close simultaneously, an LM2596 instantly drops out, the voltage collapses below 4V, and the ESP32 resets in a brownout boot loop.

The 300W 20A synchronous step-down converter handles up to 15A continuous and 20A peak without breaking a sweat, providing rock-solid 6.0V to both the servo motor rails and the ESP32 VIN pin.

---

## 3S 18650 Battery Care

- **Nominal Voltage**: 11.1V (3.7V per cell).
- **Full Charge**: 12.6V (4.2V per cell).
- **Cutoff Voltage**: 9.0V to 9.6V (3.0V to 3.2V per cell).
- Use a dedicated 3S BMS (Battery Management System) board or balance charger to avoid over-discharging cells below 3.0V.
