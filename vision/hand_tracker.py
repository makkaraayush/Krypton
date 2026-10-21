"""
Krypton Hand Tracking Pipeline
Uses MediaPipe Hands and OpenCV to track user hand articulation, compute 5-finger
flexion angles, and stream low-latency kinematics to the ESP32 bionic controller.
"""

import sys
import time
import argparse
import numpy as np
import cv2

try:
    import mediapipe as mp
except ImportError:
    print("[ERROR] MediaPipe not found. Run: pip install -r requirements.txt")
    sys.exit(1)

from communicator import ArmCommunicator
from visualizer import CyberHud


def calculate_angle_3d(a, b, c):
    """
    Computes angle (in degrees) at joint 'b' formed by segments ab and bc.
    """
    ab = np.array([a.x - b.x, a.y - b.y, a.z - b.z])
    cb = np.array([c.x - b.x, c.y - b.y, c.z - b.z])
    norm_ab = np.linalg.norm(ab)
    norm_cb = np.linalg.norm(cb)
    if norm_ab == 0 or norm_cb == 0:
        return 180.0
    cosine = np.dot(ab, cb) / (norm_ab * norm_cb)
    cosine = np.clip(cosine, -1.0, 1.0)
    angle = np.degrees(np.arccos(cosine))
    return angle


def compute_finger_flexions(landmarks, is_right_hand=True):
    """
    Computes normalized flexion (0.0 = fully open, 1.0 = fully clenched)
    and maps to servo degrees (0 to 180).
    """
    # Landmark indices for each finger (MCP, PIP, DIP, TIP)
    # Thumb: 1 (CMC), 2 (MCP), 3 (IP), 4 (TIP)
    # Index: 5 (MCP), 6 (PIP), 7 (DIP), 8 (TIP)
    # Middle: 9, 10, 11, 12
    # Ring: 13, 14, 15, 16
    # Pinky: 17, 18, 19, 20

    wrist = landmarks[0]
    middle_mcp = landmarks[9]
    # Hand scale normalization factor (wrist to middle MCP distance)
    palm_span = np.linalg.norm([wrist.x - middle_mcp.x, wrist.y - middle_mcp.y, wrist.z - middle_mcp.z])
    if palm_span == 0:
        palm_span = 0.2

    flexions = []

    # 1. Thumb Flexion
    # Measure angle at MCP (1 -> 2 -> 3) and distance from Thumb tip (4) to base of Index MCP (5)
    thumb_ang = calculate_angle_3d(landmarks[1], landmarks[2], landmarks[3])
    thumb_dist = np.linalg.norm([landmarks[4].x - landmarks[5].x, landmarks[4].y - landmarks[5].y]) / palm_span
    # Normalized thumb flexion: extended thumb has high distance (~0.9), clenched thumb has low (~0.25)
    thumb_norm = np.clip((0.85 - thumb_dist) / 0.55, 0.0, 1.0)
    flexions.append(float(thumb_norm))

    # 2-5. Index, Middle, Ring, Pinky Flexions
    finger_indices = [
        (5, 6, 7, 8),     # Index
        (9, 10, 11, 12),  # Middle
        (13, 14, 15, 16), # Ring
        (17, 18, 19, 20)  # Pinky
    ]

    for mcp, pip, dip, tip in finger_indices:
        # Distance from fingertip to wrist relative to palm size
        tip_dist = np.linalg.norm([landmarks[tip].x - wrist.x, landmarks[tip].y - wrist.y]) / palm_span
        # Straight finger tip_dist is ~1.7 - 2.0; fully bent finger is ~0.7 - 0.9
        f_norm = np.clip((1.8 - tip_dist) / 1.0, 0.0, 1.0)
        flexions.append(float(f_norm))

    return flexions


def main():
    parser = argparse.ArgumentParser(description="Krypton Real-Time Vision Teleoperation")
    parser.add_argument("--cam", type=int, default=0, help="Camera device index")
    parser.add_argument("--mode", type=str, default="udp", choices=["udp", "serial", "none"], help="Telemetry mode")
    parser.add_argument("--ip", type=str, default="192.168.4.1", help="ESP32 IP address for UDP")
    parser.add_argument("--port", type=str, default=None, help="Serial COM port (e.g. COM3) or empty for auto-detect")
    parser.add_argument("--mirror", action="store_true", default=True, help="Mirror camera view")
    args = parser.parse_args()

    print("==================================================")
    print("      KRYPTON REAL-TIME VISION TELEOPERATION      ")
    print("==================================================")
    print(f"[*] Telemetry Mode : {args.mode.upper()}")
    print(f"[*] Camera Index   : {args.cam}")
    print(f"[*] Mirror Display : {args.mirror}")
    print("--------------------------------------------------")

    cap = cv2.VideoCapture(args.cam)
    if not cap.isOpened():
        print(f"[ERROR] Could not open camera {args.cam}. Check device connection.")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

    mp_hands = mp.solutions.hands
    hands = mp_hands.Hands(
        static_image_mode=False,
        max_num_hands=1,
        min_detection_confidence=0.7,
        min_tracking_confidence=0.6
    )
    mp_draw = mp.solutions.drawing_utils
    mp_draw_styles = mp.solutions.drawing_styles

    comm = ArmCommunicator(mode=args.mode, ip=args.ip, serial_port=args.port)
    hud = CyberHud()

    # Low-pass filter state
    smoothed_angles = [0.0, 0.0, 0.0, 0.0, 0.0]
    alpha = 0.35  # Smoothing factor (higher = more responsive, lower = smoother)

    prev_time = time.time()
    fps = 30.0

    print("[*] Tracking started. Aim camera at your RIGHT HAND.")

    try:
        while cap.isOpened():
            ret, frame = cap.read()
            if not ret:
                break

            if args.mirror:
                frame = cv2.flip(frame, 1)

            curr_time = time.time()
            fps = 0.9 * fps + 0.1 * (1.0 / max(curr_time - prev_time, 0.001))
            prev_time = curr_time

            rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            results = hands.process(rgb_frame)

            flexions = [0.0, 0.0, 0.0, 0.0, 0.0]

            if results.multi_hand_landmarks:
                for hand_landmarks, handedness in zip(results.multi_hand_landmarks, results.multi_handedness):
                    # Draw skeletal connections
                    mp_draw.draw_landmarks(
                        frame,
                        hand_landmarks,
                        mp_hands.HAND_CONNECTIONS,
                        mp_draw_styles.get_default_hand_landmarks_style(),
                        mp_draw_styles.get_default_hand_connections_style()
                    )

                    # Compute 5-finger articulation
                    flexions = compute_finger_flexions(hand_landmarks.landmark)

                    # Map 0.0 - 1.0 to 0 - 180 degrees with EMA filter
                    for i in range(5):
                        target_deg = flexions[i] * 180.0
                        smoothed_angles[i] = alpha * target_deg + (1.0 - alpha) * smoothed_angles[i]

                    # Transmit to ESP32
                    comm.send_angles(
                        smoothed_angles[0],
                        smoothed_angles[1],
                        smoothed_angles[2],
                        smoothed_angles[3],
                        smoothed_angles[4]
                    )

            # Draw HUD
            hud.draw_telemetry_bars(frame, smoothed_angles, flexions)
            hud.draw_status_bar(frame, fps, comm.status_message, comm.mode)

            cv2.imshow("Krypton Bionic Arm - Vision Teleoperation", frame)

            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            elif key == ord('m'):
                new_mode = comm.cycle_mode()
                print(f"[COMM] Switched mode to: {new_mode.upper()}")
            elif key == ord('c'):
                smoothed_angles = [0.0, 0.0, 0.0, 0.0, 0.0]
                comm.send_angles(0, 0, 0, 0, 0)
                print("[COMM] Recalibrated to 0 degrees.")

    finally:
        cap.release()
        cv2.destroyAllWindows()
        comm.close()
        print("[*] Teleoperation closed cleanly.")


if __name__ == "__main__":
    main()
