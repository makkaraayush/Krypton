# 3D Printing & Slicing Hub

This directory contains the complete 3D model suite, multi-color slicing configurations, and Bambu Studio 3MF projects for Krypton.

## Guides

- [filament_palette.md](filament_palette.md): 5-spool filament palette breakdown, assigned STLs, and calibrated AMS purge volumes.
- [plates_and_slicing.md](plates_and_slicing.md): Multi-plate zero-waste print schedule, modifier geometries, and geometry notes on `topsurfaceUP6.stl`.

## Asset Folders

- `stl/fingers/`: Finger joint segments (`Index3.stl`, `Majeure3.stl`, `ringfinger3.stl`, `Auriculaire3.stl`, `thumb5.stl`).
- `stl/forearm/`: Forearm chassis spine (`robpart1V2.stl` through `robpart5V4.stl`, `robcap3V2.stl`).
- `stl/wrist/`: Wrist rotation collars, gears, and cable routing (`WristlargeV4.stl`, `WristsmallV4.stl`, `cableholderwristV4.stl`, `WristGearsV4.stl`, `rotawrist1V3` through `rotawrist3V2`, `Bolt_entretoise7.stl`).
- `stl/armor/`: Outer cyberpunk shrouds (`coverfinger1.stl`, `topsurfaceUP6.stl`, and legacy archive `topsurface6.stl`).
- `stl/drive_mechanism/`: Servo bed frame (`RobServoBedV5.stl`), cable routing combs (`RobCableFrontV3.stl`, `RobCableBackV3.stl`), winch pulleys, and electronics mount (`ardiuinosupport.stl`).
- `3mf/`: Sliced Bambu Studio production projects:
  - `Krypton_Bambu_A1_AMS_Plates.3mf`: Complete 4-plate consolidated setup.
  - `Auriculaire3.3mf`: Dedicated single-part plate configuration for pinky finger bed orientation.
