#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// Krypton Bionic Arm - Pin & Hardware Config
// ==========================================

// I2C Bus Configuration for PCA9685 PWM Driver
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define I2C_CLOCK_SPEED     400000  // Fast mode 400kHz

#define PCA9685_I2C_ADDR    0x40    // Default address with all address solder pads open
#define PWM_FREQUENCY       50      // 50Hz for analog/digital hobby servos (20ms cycle)

// PCA9685 Channel Mapping (5 MG996R Servos)
#define CH_THUMB            0
#define CH_INDEX            1
#define CH_MIDDLE           2
#define CH_RING             3
#define CH_PINKY            4

#define NUM_SERVOS          5

// Pulse Width Calibration (12-bit timer: 0 to 4095 ticks)
// For 50Hz: 1 cycle = 20,000 microseconds (20ms)
// 102 ticks ~= 500 us (0 degrees)
// 512 ticks ~= 2500 us (180 degrees)
#define SERVO_PULSE_MIN     102
#define SERVO_PULSE_MAX     512

// Per-finger angle limits (degrees: 0 = fully extended open, 180 = fully contracted fist)
const int SERVO_MIN_ANGLES[NUM_SERVOS] = { 0,   0,   0,   0,   0   };
const int SERVO_MAX_ANGLES[NUM_SERVOS] = { 180, 180, 180, 180, 180 };

// Direction Inversion (Set true if spool rotation tightens tendon in reverse direction)
const bool SERVO_INVERT[NUM_SERVOS]    = { false, false, false, false, false };

// ==========================================
// Network & Telemetry Settings
// ==========================================

// SoftAP Mode (Direct Phone/Laptop Connection without external router)
#define AP_SSID             "Krypton-Arm"
#define AP_PASSWORD         "cyberpunk2026"
#define AP_CHANNEL          1
#define AP_MAX_CONN         4

// Station Mode (Optional: connect to home Wi-Fi)
#define STA_SSID            "Your_WiFi_SSID"
#define STA_PASSWORD        "Your_WiFi_Password"
#define ENABLE_STA_MODE     false

#define HTTP_SERVER_PORT    80
#define WS_SERVER_PORT      81
#define UDP_SERVER_PORT     8888
#define SERIAL_BAUD_RATE    115200

// ==========================================
// Motion Smoothing / Trajectory Generator
// ==========================================
// Prevents abrupt jerk and tendon snaps by stepping current angle towards target
#define MOTION_SMOOTHING        true
#define TRAJECTORY_STEP_DEG     3.5f    // Maximum degrees moved per interpolation step
#define TRAJECTORY_INTERVAL_MS  15      // Easing loop update interval (~66 Hz)

#endif // CONFIG_H
