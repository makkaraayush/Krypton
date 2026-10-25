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
    wrist = landmarks[0]
    index_mcp = landmarks[5]
    middle_mcp = landmarks[9]

    # Hand scale normalization factor (stable multi-knuckle baseline)
    # Combining wrist-to-middle and wrist-to-index MCP reduces perspective foreshortening
    dist_wm = np.linalg.norm([wrist.x - middle_mcp.x, wrist.y - middle_mcp.y, wrist.z - middle_mcp.z])
    dist_wi = np.linalg.norm([wrist.x - index_mcp.x, wrist.y - index_mcp.y, wrist.z - index_mcp.z])
    palm_span = 0.5 * (dist_wm + dist_wi)
    if palm_span < 0.05:
        palm_span = 0.2

    flexions = []

    # -------------------------------------------------------------
    # 1. THUMB FLEXION (Isolating fold from lateral abduction)
    # -------------------------------------------------------------
    # Landmark indices:
    # 1: CMC, 2: MCP, 3: IP, 4: TIP, 17: Pinky MCP
    #
    # (a) IP joint angle (landmark 3 formed by 2 -> 3 -> 4):
    # Pure curling of thumb tip. When thumb is straight, angle ~ 155-175 deg.
    # When bent/curled, drops to 90-120 deg.
    ip_angle = calculate_angle_3d(landmarks[2], landmarks[3], landmarks[4])
    ip_curl = np.clip((160.0 - ip_angle) / 50.0, 0.0, 1.0)

    # (b) MCP joint angle (landmark 2 formed by 1 -> 2 -> 3):
    # Base folding into palm. Straight is ~ 150-170 deg.
    mcp_angle = calculate_angle_3d(landmarks[1], landmarks[2], landmarks[3])
    mcp_curl = np.clip((152.0 - mcp_angle) / 45.0, 0.0, 1.0)

    # (c) Opposition to pinky base (distance from 4 to 17):
    # Moving thumb sideways (abduction) increases distance (> 1.2), never falsely folding.
    # Folding thumb into palm brings tip close to pinky base (< 0.55).
    pinky_dist = np.linalg.norm([
        landmarks[4].x - landmarks[17].x,
        landmarks[4].y - landmarks[17].y,
        landmarks[4].z - landmarks[17].z
    ]) / palm_span
    opp_flex = np.clip((1.05 - pinky_dist) / 0.55, 0.0, 1.0)

    # Joint curl vs lateral move gate:
    # If the thumb joints are straight (user is just moving thumb sideways),
    # do NOT trigger a fold!
    joint_curl = max(ip_curl, 0.6 * ip_curl + 0.4 * mcp_curl)
    if joint_curl < 0.12 and opp_flex < 0.35:
        thumb_flex = 0.0
    else:
        thumb_flex = 0.5 * joint_curl + 0.5 * opp_flex

    # Resting deadband (prevents jitter or slight natural curve from reporting fold)
    if thumb_flex < 0.10:
        thumb_flex = 0.0
    else:
        thumb_flex = (thumb_flex - 0.10) / 0.90

    flexions.append(float(np.clip(thumb_flex, 0.0, 1.0)))

    # -------------------------------------------------------------
    # 2-5. INDEX, MIDDLE, RING, PINKY FLEXIONS
    # -------------------------------------------------------------
    finger_indices = [
        (5, 6, 7, 8),     # Index: MCP, PIP, DIP, TIP
        (9, 10, 11, 12),  # Middle
        (13, 14, 15, 16), # Ring
        (17, 18, 19, 20)  # Pinky
    ]

    for mcp, pip, dip, tip in finger_indices:
        # PIP joint bend angle
        pip_angle = calculate_angle_3d(landmarks[mcp], landmarks[pip], landmarks[dip])
        ang_flex = np.clip((165.0 - pip_angle) / 95.0, 0.0, 1.0)

        # 3D distance from fingertip to wrist relative to palm size
        tip_dist = np.linalg.norm([
            landmarks[tip].x - wrist.x,
            landmarks[tip].y - wrist.y,
            landmarks[tip].z - wrist.z
        ]) / palm_span
        dist_flex = np.clip((1.75 - tip_dist) / 0.95, 0.0, 1.0)

        # Weighted combination: 55% joint angle + 45% spatial distance
        f_raw = 0.55 * ang_flex + 0.45 * dist_flex

        # Rest deadband
        if f_raw < 0.08:
            f_norm = 0.0
        else:
            f_norm = (f_raw - 0.08) / 0.92

        flexions.append(float(np.clip(f_norm, 0.0, 1.0)))

    return flexions


def main():
    parser = argparse.ArgumentParser(description="Krypton Real-Time Vision Teleoperation")
    parser.add_argument("--cam", type=int, default=0, help="Camera device index")
    parser.add_argument("--mode", type=str, default="udp", choices=["udp", "serial", "none"], help="Telemetry mode")
    parser.add_argument("--ip", type=str, default="192.168.4.1", help="ESP32 IP address for UDP")
    parser.add_argument("--udp-port", type=int, default=8888, help="UDP telemetry port on ESP32 (default 8888)")
    parser.add_argument("--port", type=str, default=None, help="Serial COM port (e.g. COM3) or empty for auto-detect")
    parser.add_argument("--alpha", type=float, default=0.35, help="EMA motion smoothing factor 0.05-0.95 (default 0.35)")
    parser.add_argument("--mirror", action="store_true", default=True, help="Mirror camera view")
    args = parser.parse_args()

    print("==================================================")
    print("      KRYPTON REAL-TIME VISION TELEOPERATION      ")
    print("==================================================")
    print(f"[*] Telemetry Mode : {args.mode.upper()}")
    if args.mode == "udp":
        print(f"[*] UDP Target     : {args.ip}:{args.udp_port}")
    print(f"[*] Filter Alpha   : {args.alpha}")
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

    comm = ArmCommunicator(mode=args.mode, ip=args.ip, udp_port=args.udp_port, serial_port=args.port)
    hud = CyberHud()

    # Low-pass filter state
    smoothed_angles = [0.0, 0.0, 0.0, 0.0, 0.0]
    raw_flexions = [0.0, 0.0, 0.0, 0.0, 0.0]
    zero_offsets = [0.0, 0.0, 0.0, 0.0, 0.0]
    alpha = max(0.05, min(args.alpha, 0.95))

    notification_text = ""
    notification_until = 0.0
    show_help = False

    prev_time = time.time()
    fps = 30.0

    print("[*] Tracking started. Aim camera at your RIGHT HAND.")
    print("[*] Tip: Hold hand relaxed and press 'c' to zero-tare your rest position.")

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

            calibrated_flexions = [0.0, 0.0, 0.0, 0.0, 0.0]

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

                    # Compute raw 5-finger articulation
                    raw_flexions = compute_finger_flexions(hand_landmarks.landmark)

                    # Apply zero-tare offsets
                    for i in range(5):
                        val = raw_flexions[i] - zero_offsets[i]
                        denom = max(1.0 - zero_offsets[i], 0.01)
                        calibrated_flexions[i] = float(np.clip(val / denom, 0.0, 1.0))

                    # Map 0.0 - 1.0 to 0 - 180 degrees with EMA filter
                    for i in range(5):
                        target_deg = calibrated_flexions[i] * 180.0
                        smoothed_angles[i] = alpha * target_deg + (1.0 - alpha) * smoothed_angles[i]

                    # Transmit to ESP32
                    comm.send_angles(
                        smoothed_angles[0],
                        smoothed_angles[1],
                        smoothed_angles[2],
                        smoothed_angles[3],
                        smoothed_angles[4]
                    )

            # Check active notification
            active_notif = notification_text if curr_time < notification_until else ""

            # Draw HUD
            hud.draw_telemetry_bars(frame, smoothed_angles, calibrated_flexions)
            hud.draw_status_bar(frame, fps, comm.status_message, comm.mode, active_notif)

            if show_help:
                hud.draw_help_modal(frame)

            cv2.imshow("Krypton Bionic Arm - Vision Teleoperation", frame)

            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            elif key == ord('m'):
                new_mode = comm.cycle_mode()
                print(f"[COMM] Switched mode to: {new_mode.upper()}")
            elif key == ord('c'):
                zero_offsets = list(raw_flexions)
                notification_text = "ZERO TARE CALIBRATED"
                notification_until = time.time() + 2.5
                print(f"[CALIBRATION] Rest zero offsets set: {[round(x, 2) for x in zero_offsets]}")
            elif key == ord('r'):
                zero_offsets = [0.0, 0.0, 0.0, 0.0, 0.0]
                smoothed_angles = [0.0, 0.0, 0.0, 0.0, 0.0]
                notification_text = "CALIBRATION RESET"
                notification_until = time.time() + 2.0
                print("[CALIBRATION] Reset zero offsets to factory default.")
            elif key == ord('h'):
                show_help = not show_help

    finally:
        cap.release()
        cv2.destroyAllWindows()
        comm.close()
        print("[*] Teleoperation closed cleanly.")


if __name__ == "__main__":
    main()
