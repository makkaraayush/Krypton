# Electrical Wiring & Calibration Guide

This guide details the physical wiring and the critical buck converter calibration procedure.

## Pre-Power Buck Converter Calibration (SZBK07 / 300W 20A)

> **WARNING**: The SZBK07 300W 20A buck converter ships from the factory with its trimmer potentiometers in random positions. If you connect it directly to your PCA9685 and ESP32 without calibration, it could output 12V and destroy your servos and boards!

1. **Disconnect Everything**: Ensure no servos, ESP32, or PCA9685 boards are connected to the buck converter output.
2. **Connect Battery**: Attach your 12V battery (3S 18650 Li-ion 11.1V-12.6V or 12V pack) to `IN+` and `IN-` on the SZBK07 screw terminal blocks.
3. **Calibrate Voltage (CV)**:
   - Set your digital multimeter to DC Voltage measurement.
   - Place multimeter probes across `OUT+` and `OUT-`.
   - Using a small flathead screwdriver, turn the **CV (Constant Voltage)** brass trim pot counter-clockwise until the output reads **exactly 6.00V DC**.
4. **Calibrate Current (CC)**:
   - Turn the **CC (Constant Current)** brass trim pot **12 to 15 full turns clockwise**.
   - This opens up the current delivery path completely so the converter never throttles under high grip loads when all 5 servos pull peak current.

---

## System Connection Diagram

You can power the ESP32 using the `V+` pin on the PCA9685's left 6-pin header. This is the cleanest possible wiring setup because the entire connection between the ESP32 and PCA9685 is just a single 5-wire Dupont ribbon cable.

### 5-Wire Header Wiring (Cleanest & Recommended)

```
[ 12V Battery ]
      │
      ├──(+)──> [IN+] SZBK07 300W 20A Buck [OUT+ (6.0V)] ──> [PCA9685 Green Screw V+]
      └──(–)──> [IN-] SZBK07 300W 20A Buck [OUT- (GND)]  ──> [PCA9685 Green Screw GND]

[ PCA9685 Left 6-Pin Header ]                   [ ESP32 38-Pin / Type-C ]
  ├── V+  (6.0V Motor Rail) ───────────────────> VIN / 5V Pin  (Powers ESP32 onboard LDO)
  ├── GND (Common Ground)   ───────────────────> GND Pin
  ├── VCC (3.3V Logic Rail) <─────────────────── 3.3V Pin (Powers PCA9685 chip)
  ├── SDA (I2C Data)        <─────────────────── GPIO 21
  ├── SCL (I2C Clock)       <─────────────────── GPIO 22
  └── OE  (Output Enable)   (Leave Disconnected)

[ PCA9685 3-Pin Servo Headers (Channels 0 to 4) ]
  ├── Channel 0 ──> Thumb MG996R Servo
  ├── Channel 1 ──> Index MG996R Servo
  ├── Channel 2 ──> Middle (Majeure) MG996R Servo
  ├── Channel 3 ──> Ring MG996R Servo
  └── Channel 4 ──> Pinky (Auriculaire) MG996R Servo
```

> **CRITICAL PIN CAUTION**:
> - **`V+`** is **6.0V**: Must connect to ESP32 **`VIN` (labeled `5V` or `V5` on USB Type-C boards)**. This pin feeds the onboard low-dropout regulator (LDO) which drops the 6.0V down to 3.3V for the ESP32 chip.
> - **`VCC`** is **3.3V**: Must connect to ESP32 **`3.3V`**. (Powers PCA9685 digital logic).
> - **DO NOT SWAP THEM**: Never connect the 6.0V `V+` pin to ESP32 `3.3V`, or you will destroy the ESP32 chip.
> - **Capacitor**: Add a **1000uF 16V capacitor** to the PCA9685 capacitor footprint (`C1`) next to the green terminal. This cushions transient voltage dips when all 5 servos pull peak current, preventing ESP32 brownouts.

---

## Pinout Table

### ESP32 to PCA9685

| ESP32 Pin | PCA9685 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| `3.3V` | `VCC` | Logic Power | **Do not** connect to 5V/6V! PCA9685 logic must match ESP32 3.3V logic. |
| `GND` | `GND` | Common Ground | Essential for reliable I2C signaling. |
| `GPIO 21` | `SDA` | I2C Serial Data | 400kHz fast mode supported. |
| `GPIO 22` | `SCL` | I2C Serial Clock | 400kHz fast mode supported. |

### Buck Converter to Boards

| Buck Output | Destination | Notes |
| :--- | :--- | :--- |
| `OUT+ (6.0V)` | `PCA9685 V+` (Green Screw Terminal) | Powers servo motor rails. Use 18/20 AWG wire. |
| `OUT+ (6.0V)` | `ESP32 VIN` | Onboard ESP32 LDO steps 6.0V down to 3.3V cleanly. |
| `OUT- (GND)` | `PCA9685 GND` (Green Screw Terminal) | High-current ground return. |
| `OUT- (GND)` | `ESP32 GND` | Common ground reference. |

---

## PCA9685 Servo Channel Assignment

| Channel | Finger | Mechanical Joint | Motion Direction |
| :---: | :--- | :--- | :--- |
| `0` | Thumb | CMC / MCP / IP Flexion | Spool pulls palmar tendon |
| `1` | Index | MCP / PIP / DIP Flexion | Spool pulls palmar tendon |
| `2` | Middle | MCP / PIP / DIP Flexion | Spool pulls palmar tendon |
| `3` | Ring | MCP / PIP / DIP Flexion | Spool pulls palmar tendon |
| `4` | Pinky | MCP / PIP / DIP Flexion | Spool pulls palmar tendon |

---

## Wire Harness & Servo Lead Strain Relief

1. **Lead Bundling**: Group the 3-wire servo leads (Brown/GND, Red/V+, Orange/PWM) along the underside channel of `RobServoBedV5.stl`.
2. **Channel Clearance**: Keep all wires away from the rotating winch spool paths. The 110 lb braided lines move rapidly and will wear down exposed insulation if permitted to rub.
3. **Chassis Anchor**: Secure the main 5-servo harness to the tie-off slots on `robpart1V2` using 2.5mm nylon zip-ties. This prevents mechanical stress from transferring to the PCA9685 header pins when the arm moves.
4. **Capacitor Buffer**: For heavy continuous grasping loads, solder a 1000uF 16V low-ESR electrolytic capacitor across the PCA9685 screw terminals (`V+` and `GND`) to absorb instantaneous inductive switching spikes.
