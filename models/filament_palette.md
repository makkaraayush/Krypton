# 5-Spool Filament Palette (Bambu Lab A1 + AMS Lite)

Rather than redesigning the joint hinges from scratch (which introduces friction and joint binding), Krypton uses verified V4 mechanical joint geometry paired with high-contrast multi-color armor plating on the Bambu Lab A1 + AMS Lite.

## Spool Assignments

| Spool ID | Color & Type | Role | Assigned STL Files |
| :---: | :--- | :--- | :--- |
| **Spool 1** | Solid Black PLA | Internal moving skeleton, outer wrist bearing ring, and recessed screw counterbores | `Index3.stl`, `Majeure3.stl`, `ringfinger3.stl`, `Auriculaire3.stl`, `thumb5.stl`, `WristlargeV4.stl`, `robcap3V2.stl` |
| **Spool 2** | Dark Grey PLA | Internal forearm chassis spine and inner rotating wrist collar | `robpart2V4.stl`, `robpart3V4.stl`, `robpart4V4.stl`, `robpart5V4.stl`, `WristsmallV4.stl` |
| **Spool 3** | Dark Blue PLA | Primary outer cybernetic armor shells | `coverfinger1.stl` (4x finger caps + 1x dorsal hand plate), `topsurfaceUP6.stl` |
| **Spool 4** | White-to-Blue Glow PLA | Luminescent energy strips, forearm conduits, custom debossed text | Embedded modifiers on `coverfinger1.stl`, custom text & grooves on `topsurfaceUP6.stl` |
| **Spool 5** | Yellow PLA | Industrial machinery accents, pulleys, spacer sleeves, electronics brackets | `ardiuinosupport.stl`, `Bolt_entretoise7.stl`, 5x `ServoPulleyHitecV5_tighter.stl`, knuckle side bosses |

---

## Bambu Studio Flushing Matrix

To avoid glow filament discoloration from dark black and dark blue pigments, set custom purge volumes (Flushing Multiplier: 1.0):

| From \ To | Black (1) | Dark Grey (2) | Dark Blue (3) | Glow (4) | Yellow (5) |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Black (1)** | - | 180 | 180 | **410** | **410** |
| **Dark Blue (3)**| 125 | 160 | - | **350** | **310** |
| **Glow (4)** | 130 | 140 | 190 | - | 160 |
| **Yellow (5)** | 140 | 160 | 190 | 220 | - |

- **Flushing Options**: Enable *Flush into objects' infill* and *Flush into objects' support* to minimize purge tower waste.

<!-- Slicer Note: AMS purge volumes tuned on Bambu Studio 1.8+ -->
