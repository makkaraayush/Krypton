# Master 4-Plate Print Plan & Slicing

To eliminate multi-color purging on structural load-bearing parts, Plates 1, 2, and 3 run as single-color batches. Only Plate 4 uses multi-color filament swaps through the AMS Lite.

## 4-Plate Master Schedule

| Plate | Filament | Parts on Plate | Layer Height | Walls & Infill | Supports & Brim | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Plate 1: Skeleton & Wrist** | Black Only | `Index3`, `Majeure3`, `ringfinger3`, `Auriculaire3`, `thumb5`, `WristlargeV4`, `cableholderwristV4`, `robcap3V2` | 0.20 mm | 5 Walls, 30% Gyroid | Supports OFF, Auto Brim | Standard speed, Seam: Back, Smooth Timelapse ON |
| **Plate 2: Forearm Chassis** | Dark Grey Only | `robpart1V2`, `robpart2V4`, `robpart3V4`, `robpart4V4`, `robpart5V4` (standing), `WristsmallV4` (flat), `RobServoBedV5` | 0.20 mm (Initial: 0.24 mm) | 5 Walls, 30% Gyroid | Tree Organic (38°, Build plate only, XY gap 0.5 mm), 8 mm Outer Brim | Standard speed, Seam: Back |
| **Plate 3: Hardware & Winches**| Yellow & Grey | `ardiuinosupport`, `Bolt_entretoise7`, `RobCableFrontV3`, `RobCableBackV3`, `WristGearsV4`, `rotawrist1V3`, `rotawrist2`, `rotawrist3V2`, 5x `ServoPulleyHitecV5_tighter.stl` | 0.20 mm | 4 Walls, 30% Gyroid | Tree Organic for wrist gears, 3 mm Outer Brim | Standard speed, Seam: Back |
| **Plate 4: Cyber Armor** | 1: Dark Blue<br>2: Glow<br>3: Yellow<br>4: Black | `coverfinger1` (split into 5 parts), `topsurfaceUP6` | 0.12 mm | 4 Walls, 6 Top / 5 Bottom layers, 25% Gyroid | Tree Organic (35°, Build plate only, Top Z 0.20 mm, XY 0.5 mm) | Outer Wall: 80-100 mm/s<br>Top Surface: 60-80 mm/s<br>Small Perim: 30 mm/s<br>Ironing: OFF |

---

## Geometry Notes: Discarding `topsurface6.stl` in Favor of `topsurfaceUP6.stl`

During dry fitting of the forearm assembly, we found that the original `topsurface6.stl` shroud rubbed against the outer rotation radius of the MG996R servo horn winches. 

`topsurfaceUP6.stl` is the updated model featuring:
1. **Recessed servo horn cutouts** giving 1.8mm extra radial clearance.
2. **Removed stock carved logo**: The sunken lettering was cleaned in Blender using loop-bridging and sculpt smoothing.
3. **Custom debossed text & grooves**: Applied in Bambu Studio using the Text Shape Tool (T) (Thickness: 0.8 mm, Embedded Depth: -0.5 mm) assigned to Spool 4 (Glow).

---

## Finger Cover Glow Modifiers (`coverfinger1.stl`)

To create the luminescent energy conduits on the finger caps:
1. Split `coverfinger1.stl` into 5 discrete objects (Right-Click -> *Split* -> *To Objects*).
2. Add a Cube Modifier ($X: 2.5	ext{ mm}, Y: 22	ext{ mm}, Z: 1.0	ext{ mm}$) to each finger cap.
3. Position the modifier embedded $0.8	ext{ mm}$ into the top dome of each finger cap.
4. Set the modifier object filament to **Spool 4 (Glow PLA)**.
