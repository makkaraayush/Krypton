# Troubleshooting Guide

### 1. ESP32 Reboots or Brownouts when Fingers Move
- **Cause**: The 300W buck converter current limit (CC) pot is throttled, or the battery is drained.
- **Fix**: Turn the CC brass trim pot 12-15 full turns clockwise to open maximum current delivery. Ensure your 3S battery is above 11.1V.

### 2. Finger Binds or Moves Stiffly
- **Cause**: The M3 knuckle pivot screw is overtightened, pinching the plastic joint faces.
- **Fix**: Loosen the M3 Nyloc nut by 1/4 to 1/2 turn. The joint must flop freely under gravity. Also ream the internal tendon channel with a 1.5mm drill bit to remove stringing.

### 3. Servo Jitters or Buzzes at Rest
- **Cause**: Tendon is strung too tight, holding the servo horn just short of its internal target position.
- **Fix**: Give the tendon 1mm of slack or adjust the software zero point in `config.h`.

### 4. PCA9685 Not Detected (`0x40 FAILED`)
- **Cause**: Loose I2C Dupont jumper or missing common ground.
- **Fix**: Ensure ESP32 GPIO 21 goes to SDA, GPIO 22 goes to SCL, and both boards share a solid GND connection.

### 5. Tendon Knot Slips Under High Tension Grasping
- **Cause**: Braided PE polyethylene line is extremely slick and standard overhand knots slip under heavy grip loads.
- **Fix**: Always tie off using a Palomar Knot or double Figure-8 knot at both the pulley spool and the fingertip anchor. Apply a single drop of medium CA superglue directly onto the knot core and let it cure fully before trimming the tag end.

### 6. Finger Jams Halfway Through Flexion Sweep
- **Cause**: The braided line is snagging on internal layer line bridges inside the `RobCableFrontV3` routing comb or knuckle channels.
- **Fix**: Pass a piece of stiff 0.8mm wire or a 1.5mm hand twist drill through the channel to clear stringing, or apply a drop of PTFE dry lubricant along the tendon pathway.

### 7. Pulley Hub Cracking or Retaining Screw Shearing Under Tension
- **Cause**: Trying to clamp down on slippery 110 lb braided line by over-torquing M3 screws into 3D-printed plastic hubs causes layer shear and fastener breakage.
- **Fix**: Reprint pulleys with 4+ perimeter walls and 40% gyroid infill (`ServoPulleyHitecV5_tighter.stl`). Switch to the 180-degree rotational tensioning technique detailed in the assembly guide and main README, which utilizes spool wrap geometry to lock line tension rather than mechanical clamping force.
