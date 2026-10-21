# Mechanical Assembly & Tendon Rigging Sequence

This guide covers the assembly of the Krypton bionic arm from freshly printed plastic parts to a fully tensioned, articulated robotic hand.

---

## Step 1: Hole Reaming & Channel Preparation

> **IMPORTANT**: 3D printed holes contract slightly during cooling. Forcing screws into undersized holes will crack the plastic or cause stiff joint binding.

1. **Ream Knuckle Pivot Holes**:
   - Take a **3.2 mm drill bit** (or hand reamer).
   - By hand (without a power drill), twist the bit through all finger knuckle hinge holes (`Index3`, `Majeure3`, `ringfinger3`, `Auriculaire3`, `thumb5`).
   - The M3 x 20mm screw must slide through the hinge holes smoothly with zero friction, acting like a polished axle pin.
2. **Clear Tendon Channels**:
   - Take a **1.5 mm to 2.0 mm drill bit**.
   - Hand-twist it through all internal palmar and dorsal tendon channels inside each finger segment.
   - Clear out any internal stringing or layer bumps so the 0.60mm braided line glides freely.

---

## Step 2: Knuckle Pinning & Friction Adjustment

1. Connect the finger segments together using the **M3 x 20mm black-oxide socket head screws**.
2. Thread an **M3 Nyloc nut** onto each screw.
3. Tighten the nut until it gently touches the plastic, then **back it off 1/4 to 1/2 turn**.
4. **Gravity Test**: Hold the hand upright and let go of the finger. Each finger joint must flop down freely under its own weight. If a joint sticks, loosen the Nyloc nut slightly.

---

## Step 3: Forearm Chassis & Servo Installation

1. Bolt forearm sections `robpart2V4`, `robpart3V4`, `robpart4V4`, and `robpart5V4` together using M3 screws.
2. Mount the 5x **TowerPro MG996R servos** into their forearm mounting beds using **M3 x 12mm screws**.
3. Assemble the rotating wrist collar (`WristlargeV4`, `WristsmallV4`, `robcap3V2`) using the M8 x 80mm, 60mm, and 40mm bolts, washers, and M8 Nyloc nuts.

---

## Step 4: Servo Zeroing (DO NOT SKIP!)

> **WARNING**: Never screw the tendon winches onto the servos before zeroing! If a servo powers on at 180 degrees while strung tight, it will snap the braided line or strip the internal gear teeth.

1. Power on the ESP32 and PCA9685.
2. Open the web dashboard or send `POSE:OPEN` via serial to command all 5 channels to **0 degrees (fully open)**.
3. With the servos energized at 0 degrees, press the yellow `ServoPulleyHitecV5_tighter.stl` winches onto the servo horns with their string tie-off holes aligned with the forearm axis.
4. Fasten the central servo horn retaining screw.

---

## Step 5: Tendon Rigging Sequence

1. **Prepare Braided Line**:
   - Cut five 1-meter lengths of **110 lb (0.60mm) PE braided fishing line**.
2. **Anchor to Pulley**:
   - Thread one end of the line through the hole in the servo winch pulley and tie a triple knot.
3. **Route Line Through Forearm**:
   - Pass the line through the wrist spacer sleeve (`Bolt_entretoise7.stl`).
   - Guide the line up into the palmar channel of the corresponding finger.
4. **Anchor to Fingertip**:
   - Pass the line out of the tip of the finger.
   - Hold the finger completely flat and straight.
   - Pull the line taut until there is zero slack, but not so tight that it lifts the servo horn.
   - Tie off the line using a **Palomar Knot** or **Figure-8 Knot**.
   - Apply a drop of **CA superglue (Fevikwik)** directly onto the knot. Let it cure for 5 minutes before cutting the excess tag end.
5. **Dorsal Return Line (Elastic or Second Tendon)**:
   - For active extension, route a return line along the dorsal channel back to the opposite side of the servo pulley winch, or use 1mm elastic shock cord inside the dorsal groove to provide passive return.

---

## Step 6: Cyber Armor Installation

1. Snap the custom-colored `coverfinger1` armor caps over each finger segment.
2. Fasten the `topsurfaceUP6` forearm shroud over the chassis using M3 black-oxide screws.
3. Verify that all 5 servo horn pulleys rotate through their full 180-degree sweep without touching the underside of the armor shroud.
