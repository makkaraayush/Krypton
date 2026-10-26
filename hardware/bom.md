# Bill of Materials (BOM)

This is the verified bill of materials for Krypton. Total costs reflect local market sourcing (e.g., Indian maker suppliers / Robu / Amazon) optimized for reliability and budget.

---

## 1. Mechanical Fasteners (Option B Optimized)

Instead of using custom ground dowel pins (which cost ₹5.20 each and easily slide out under vibration), we use M3 x 20mm high-tensile black-oxide socket head screws paired with M3 Nyloc nuts. The unthreaded shank of the screw acts as a low-friction axle inside the 3.2mm reamed knuckle holes, and the nylon collar prevents loosening while allowing precise tension adjustment.

| Fastener | Quantity | Spec / Grade | Purpose | Est. Cost (INR) |
| :--- | :---: | :--- | :--- | :---: |
| M3 x 20mm Socket Screws | 30 | High-Tensile 12.9 Black-Oxide | 16 for finger knuckles, 10 for forearm chassis (`robpart1V2` to `robpart5V4`), 4 spares | ₹78.00 |
| M3 Nyloc Nuts | 30 | SS304 DIN985 | 16 for knuckles, 8 for chassis spine, 6 spares | ₹66.00 |
| M3 x 12mm Socket Screws | 26 | High-Tensile 12.9 Black-Oxide | 20 for mounting 5 servos into `RobServoBedV5`, 4 for `RobCable` combs & electronics, 2 spares | ₹52.00 |
| M8 x 80mm Socket Screws | 2 | Black-Oxide Grade 8.8 | Connects WristlargeV4 to WristsmallV4 (1 required + 1 spare) | ₹40.00 |
| M8 x 60mm Socket Screws | 2 | Black-Oxide Grade 8.8 | Connects WristlargeV4 to forearm base (1 required + 1 spare) | ₹23.60 |
| M8 x 40mm Socket Screws | 2 | Black-Oxide Grade 8.8 | Connects WristlargeV4 to thumb base pivot (1 required + 1 spare) | ₹16.00 |
| M8 Nyloc Nuts | 4 | SS304 DIN982 | 3 required for M8 bolts, 1 spare | ₹30.40 |
| M8 Plain Washers | 6 | GI Plated (ID 8.2mm, OD 17mm) | Distributes clamp pressure across printed plastic | ₹4.80 |
| **Total Fasteners** | | | | **₹310.80** |

---

## 2. Tendon Lines & Consumables

| Item | Spec | Purpose | Est. Cost (INR) |
| :--- | :--- | :--- | :---: |
| Braided PE Fishing Line | PROBEROS 110 lb (50 kg) / 0.60mm 4-strand (100M) | Near-zero stretch tendon cables for palmar flexion and dorsal return | ₹380.00 |
| CA Superglue | Cyanoacrylate (Fevikwik / Gorilla Super Glue) | Locking Palomar/Figure-8 knots on braided line | ₹30.00 |
| Heat Shrink Tubing | 4mm Black Polyolefin | Insulating solder joints and wire bundles | ₹40.00 |

---

## 3. Electronics & Power

| Component | Quantity | Spec | Purpose | Est. Cost (INR) |
| :--- | :---: | :--- | :--- | :---: |
| Microcontroller | 1 | ESP32 Type-C CP2102 38-Pin | Wi-Fi / BLE dual-core master controller | ₹429.00 |
| PWM Driver | 1 | PCA9685 16-Channel 12-Bit I2C | Generates jitter-free 50Hz PWM pulses | ₹275.00 |
| Servos | 5 | TowerPro MG996R (180-deg positional) | Metal-gear finger actuation (11 kg-cm @ 6.0V) | ₹1,745.00 |
| Buck Converter | 1 | 300W 20A DC-DC Step-Down | Regulates 3S battery down to 6.0V without brownout | ₹335.00 |
| Hookup Wire | 2m | 18 AWG / 20 AWG Silicone Wire | High-current battery and buck converter feed | ₹90.00 |
| Dupont Jumpers | 10 | Female-to-Female 10cm | I2C logic lines between ESP32 and PCA9685 | ₹35.00 |
| DC Barrel Terminal | 1 | DC Female Barrel to Screw Terminal | Clean power input connection | ₹25.00 |
| Battery (Existing) | 1 | 3S 18650 Li-ion Pack (11.1V - 12.6V) | Primary mobile power source | (Existing) |
