"""
Krypton Hand Tracking Pipeline
Uses MediaPipe Hands and OpenCV to track user hand articulation, compute 6-channel
kinematics (5 fingers + wrist rotation), and stream low-latency commands to the ESP32 controller.
Supports auto-detection of USB Serial and Wi-Fi UDP, and progressive calibration curve profiles.
"""

import sys
import os
import time
import argparse
import numpy as np
import cv2

# Ensure local imports work whether executed from repo root or vision folder
_VISION_DIR = os.path.dirname(os.path.abspath(__file__))
if _VISION_DIR not in sys.path:
    sys.path.insert(0, _VISION_DIR)

try:
    import mediapipe as mp
except ImportError:
    print("[ERROR] MediaPipe not found. Run: pip install -r requirements.txt")
    sys.exit(1)

from communicator import ArmCommunicator
from visualizer import CyberHud
import calibration


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
    for Thumb and 4 fingers based on true 3D horizontal kinematics and knuckle curl.
    """
    wrist = landmarks[0]
    index_mcp = landmarks[5]
    middle_mcp = landmarks[9]

    # Hand scale normalization factor (stable multi-knuckle baseline)
    dist_wm = np.linalg.norm([wrist.x - middle_mcp.x, wrist.y - middle_mcp.y, wrist.z - middle_mcp.z])
    dist_wi = np.linalg.norm([wrist.x - index_mcp.x, wrist.y - index_mcp.y, wrist.z - index_mcp.z])
    palm_span = 0.5 * (dist_wm + dist_wi)
    if palm_span < 0.05:
        palm_span = 0.2

    flexions = []

    # -------------------------------------------------------------
    # 1. THUMB FLEXION (Horizontal Sweep Across the Fingers)
    # -------------------------------------------------------------
    # Human thumb does not fold down into the base; it sweeps horizontally
    # across the front/outside of the curled fingers toward the pinky side.
    v_index_mcp = np.array([landmarks[5].x, landmarks[5].y, landmarks[5].z])
    v_pinky_mcp = np.array([landmarks[17].x, landmarks[17].y, landmarks[17].z])
    v_thumb_tip = np.array([landmarks[4].x, landmarks[4].y, landmarks[4].z])
    v_thumb_mcp = np.array([landmarks[2].x, landmarks[2].y, landmarks[2].z])
    v_index_pip = np.array([landmarks[6].x, landmarks[6].y, landmarks[6].z])
    v_middle_pip = np.array([landmarks[10].x, landmarks[10].y, landmarks[10].z])

    # Across-palm vector pointing horizontally from Index MCP toward Pinky MCP
    vec_across = v_pinky_mcp - v_index_mcp
    norm_across = np.linalg.norm(vec_across)
    u_across = vec_across / (norm_across if norm_across > 1e-4 else 1.0)

    # (a) Horizontal progress: projection of Thumb Tip relative to Index MCP
    # When open/extended: Thumb Tip is to the outside (negative projection, ~ -0.6 to -0.8 palm spans)
    # When neutral/adjacent: projection is ~ -0.1 to 0.0
    # When folded across fingers (fist): Thumb Tip reaches across Index/Middle (positive projection, ~ +0.15 to +0.40)
    proj_across = np.dot(v_thumb_tip - v_index_mcp, u_across) / palm_span
    horiz_progress = np.clip((proj_across - (-0.45)) / 0.70, 0.0, 1.0)

    # (b) Proximity above the fingers:
    # When forming a fist, Thumb wraps above the fingers (near midpoint of Index PIP & Middle PIP)
    fingers_mid = 0.5 * (v_index_pip + v_middle_pip)
    dist_over_fingers = np.linalg.norm(v_thumb_tip - fingers_mid) / palm_span
    # Open: dist ~ 0.85-1.2. Clamped over fingers: dist drops to ~ 0.18-0.35.
    prox_progress = np.clip((0.85 - dist_over_fingers) / 0.55, 0.0, 1.0)

    # (c) Thumb vector horizontal alignment:
    # Vector from Thumb MCP to Thumb Tip rotates from pointing outward to pointing across toward Pinky
    t_vec = v_thumb_tip - v_thumb_mcp
    t_norm = np.linalg.norm(t_vec)
    if t_norm > 1e-4:
        align_progress = np.clip((np.dot(t_vec / t_norm, u_across) - (-0.25)) / 0.85, 0.0, 1.0)
    else:
        align_progress = 0.0

    # (d) Thumb IP joint curl (landmarks 2 -> 3 -> 4)
    ip_angle = calculate_angle_3d(landmarks[2], landmarks[3], landmarks[4])
    ip_curl = np.clip((165.0 - ip_angle) / 55.0, 0.0, 1.0)

    # Combined thumb flexion: prioritizes horizontal travel across the palm and resting over fingers
    thumb_raw = 0.45 * horiz_progress + 0.30 * prox_progress + 0.15 * align_progress + 0.10 * ip_curl

    if thumb_raw < 0.08:
        thumb_norm = 0.0
    else:
        # Reaches 1.0 smoothly at full horizontal fist fold (thumb_raw >= 0.78)
        thumb_norm = np.clip((thumb_raw - 0.08) / 0.70, 0.0, 1.0)

    flexions.append(float(thumb_norm))

    # -------------------------------------------------------------
    # 2-5. INDEX, MIDDLE, RING, PINKY FLEXIONS
    # -------------------------------------------------------------
    # Uses self-normalizing anatomical finger segment lengths (MCP->PIP->DIP->TIP).
    # Adapts automatically to individual finger sizes so shorter fingers like the Pinky
    # achieve true 0.0 when fully open and solid 1.0 when clenched into a fist.
    finger_indices = [
        (5, 6, 7, 8),     # Index: MCP, PIP, DIP, TIP
        (9, 10, 11, 12),  # Middle
        (13, 14, 15, 16), # Ring
        (17, 18, 19, 20)  # Pinky
    ]

    for mcp_idx, pip_idx, dip_idx, tip_idx in finger_indices:
        p_mcp = landmarks[mcp_idx]
        p_pip = landmarks[pip_idx]
        p_dip = landmarks[dip_idx]
        p_tip = landmarks[tip_idx]

        v_mcp = np.array([p_mcp.x, p_mcp.y, p_mcp.z])
        v_pip = np.array([p_pip.x, p_pip.y, p_pip.z])
        v_dip = np.array([p_dip.x, p_dip.y, p_dip.z])
        v_tip = np.array([p_tip.x, p_tip.y, p_tip.z])

        # Self-normalized anatomical finger bone length:
        # Sum of the 3 phalanx segments: (MCP->PIP) + (PIP->DIP) + (DIP->TIP)
        # Dynamically scales to each finger's physical proportions, eliminating open-pinky offsets
        seg_prox = np.linalg.norm(v_pip - v_mcp)
        seg_mid  = np.linalg.norm(v_dip - v_pip)
        seg_dist = np.linalg.norm(v_tip - v_dip)
        finger_full_len = seg_prox + seg_mid + seg_dist
        if finger_full_len < 1e-4:
            finger_full_len = palm_span

        # Distance from Tip to MCP relative to the finger's own total extended length:
        # Fully open / extended straight: extension_ratio is ~ 0.94 - 1.0 (for ALL fingers)
        # Closed in a fist: extension_ratio drops to ~ 0.22 - 0.35
        extension_ratio = np.linalg.norm(v_tip - v_mcp) / finger_full_len
        dist_flex = np.clip((0.92 - extension_ratio) / 0.60, 0.0, 1.0)

        # PIP joint curl (knuckle bend)
        pip_angle = calculate_angle_3d(p_mcp, p_pip, p_dip)
        pip_flex = np.clip((165.0 - pip_angle) / 95.0, 0.0, 1.0)

        # DIP joint curl
        dip_angle = calculate_angle_3d(p_pip, p_dip, p_tip)
        dip_flex = np.clip((160.0 - dip_angle) / 75.0, 0.0, 1.0)

        f_raw = 0.50 * dist_flex + 0.35 * pip_flex + 0.15 * dip_flex

        if f_raw < 0.08:
            f_norm = 0.0
        else:
            # Reaches solid 1.0 (180 deg) at f_raw >= 0.78 and STAYS at 1.0 under tightest fist squeeze
            f_norm = np.clip((f_raw - 0.08) / 0.70, 0.0, 1.0)

        flexions.append(float(f_norm))

    # -------------------------------------------------------------
    # THUMB-OVER-INDEX OCCLUSION COMPENSATION
    # -------------------------------------------------------------
    # When the thumb folds horizontally over the index finger in a fist:
    # Camera vision occludes the index DIP/TIP, which can cause MediaPipe to misestimate
    # occluded index landmarks and falsely drop index angle from 180 deg to ~150 deg.
    # If the thumb is wrapped across the palm (thumb_norm > 0.40) and covering index,
    # and either index was already curled or middle finger is closed in a fist,
    # clamp index solidly to 1.0 (180 deg).
    thumb_val = flexions[0]
    index_val = flexions[1]
    mid_val   = flexions[2]

    if thumb_val > 0.40 and horiz_progress > 0.35:
        dist_thumb_to_index_pip = np.linalg.norm(v_thumb_tip - v_index_pip) / palm_span
        if dist_thumb_to_index_pip < 0.48:
            # Thumb is directly covering the index finger
            if index_val > 0.65 or mid_val > 0.70:
                # Index is curled in a fist under the thumb -> lock to 1.0 (180 deg)
                flexions[1] = 1.0

    return flexions


def compute_wrist_roll(landmarks) -> float:
    """
    Computes hand roll / tilt in degrees mapped to 0-180 for wrist rotation servo.
    Neutral (palm flat / vertical) = 90.0 degrees.
    Tilted left = 0.0 to 90.0 degrees.
    Tilted right = 90.0 to 180.0 degrees.
    """
    index_mcp = landmarks[5]
    pinky_mcp = landmarks[17]

    dx = pinky_mcp.x - index_mcp.x
    dy = pinky_mcp.y - index_mcp.y

    roll_rad = np.arctan2(dy, dx)
    roll_deg = np.degrees(roll_rad)

    # Center around horizontal (roll_deg ~ 0 when palm flat in mirrored view)
    wrist_target = 90.0 + np.clip(roll_deg * 1.5, -90.0, 90.0)
    return float(np.clip(wrist_target, 0.0, 180.0))


def main():
    parser = argparse.ArgumentParser(description="Krypton Real-Time Vision Teleoperation")
    parser.add_argument("--cam", type=int, default=0, help="Camera device index")
    parser.add_argument("--mode", type=str, default="auto", choices=["auto", "serial", "udp", "none"], help="Telemetry mode (default: auto)")
    parser.add_argument("--ip", type=str, default="192.168.4.1", help="ESP32 IP address for UDP")
    parser.add_argument("--udp-port", type=int, default=8888, help="UDP telemetry port on ESP32 (default 8888)")
    parser.add_argument("--port", type=str, default=None, help="Serial COM port (e.g. COM3) or empty for auto-detect")
    parser.add_argument("--alpha", type=float, default=0.75, help="EMA motion smoothing factor 0.05-0.95 (default 0.75)")
    parser.add_argument("--mirror", action="store_true", default=True, help="Mirror camera view")
    parser.add_argument("--wrist", action="store_true", default=False, help="Enable live wrist roll tracking on boot")
    args = parser.parse_args()

    # Load custom calibration profile
    profile = calibration.load_profile()
    channel_keys = calibration.CHANNEL_KEYS

    print("==================================================")
    print("      KRYPTON REAL-TIME VISION TELEOPERATION      ")
    print("==================================================")
    print(f"[*] Telemetry Mode : {args.mode.upper()}")
    if args.mode in ["auto", "udp"]:
        print(f"[*] UDP Target     : {args.ip}:{args.udp_port}")
    print(f"[*] Filter Alpha   : {args.alpha}")
    print(f"[*] Camera Index   : {args.cam}")
    print(f"[*] Mirror Display : {args.mirror}")
    print(f"[*] Wrist Tracking : {'ON' if args.wrist else 'LOCKED (90 deg)'}")
    print(f"[*] Calibration    : Loaded progressive curve profile")
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

    # Low-pass filter state (6 channels: T, I, M, R, P, W)
    smoothed_angles = [0.0, 0.0, 0.0, 0.0, 0.0, 90.0]
    raw_flexions = [0.0, 0.0, 0.0, 0.0, 0.0]
    zero_offsets = [0.0, 0.0, 0.0, 0.0, 0.0]
    alpha = max(0.05, min(args.alpha, 0.95))
    track_wrist = args.wrist

    notification_text = ""
    notification_until = 0.0
    show_help = False

    prev_time = time.time()
    fps = 30.0

    print("[*] Tracking started. Aim camera at your RIGHT HAND.")
    print("[*] Tip: Hold hand relaxed and press 'c' to zero-tare your rest position.")
    print("[*] Press 'w' to toggle wrist roll tracking.")

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

            calibrated_flexions = [0.0, 0.0, 0.0, 0.0, 0.0, 0.5]

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

                    # Progressive non-linear angle conversion using calibrated profile
                    for i in range(5):
                        ch_key = channel_keys[i]
                        target_deg = calibration.map_flexion_to_angle(calibrated_flexions[i], profile[ch_key])
                        # Adaptive low-latency filter:
                        # Quick gesture snap (delta > 8 deg): snap at dynamic alpha 0.88-0.92 for near real-time tracking
                        # Holding static pose (delta < 2.5 deg): smooth at lower alpha to eliminate camera landmark noise
                        delta = abs(target_deg - smoothed_angles[i])
                        if delta > 10.0:
                            dyn_alpha = 0.90
                        elif delta > 4.0:
                            dyn_alpha = 0.70
                        else:
                            dyn_alpha = min(alpha, 0.35)
                        smoothed_angles[i] = dyn_alpha * target_deg + (1.0 - dyn_alpha) * smoothed_angles[i]

                    # Wrist roll tracking
                    if track_wrist:
                        raw_wrist_deg = compute_wrist_roll(hand_landmarks.landmark)
                        w_delta = abs(raw_wrist_deg - smoothed_angles[5])
                        w_alpha = 0.88 if w_delta > 10.0 else (0.65 if w_delta > 4.0 else 0.35)
                        smoothed_angles[5] = w_alpha * raw_wrist_deg + (1.0 - w_alpha) * smoothed_angles[5]
                    else:
                        smoothed_angles[5] = 90.0

                    calibrated_flexions[5] = float(np.clip(smoothed_angles[5] / 180.0, 0.0, 1.0))

                    # Transmit all 6 channels to ESP32
                    comm.send_angles(
                        smoothed_angles[0],
                        smoothed_angles[1],
                        smoothed_angles[2],
                        smoothed_angles[3],
                        smoothed_angles[4],
                        smoothed_angles[5]
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
                notification_text = f"MODE: {new_mode.upper()}"
                notification_until = time.time() + 2.0
                print(f"[COMM] Switched mode to: {new_mode.upper()}")
            elif key == ord('w'):
                track_wrist = not track_wrist
                notification_text = f"WRIST: {'LIVE TRACK' if track_wrist else 'LOCKED 90 DEG'}"
                notification_until = time.time() + 2.0
                print(f"[WRIST] Wrist tracking: {'ENABLED' if track_wrist else 'LOCKED AT 90 DEG'}")
            elif key == ord('c'):
                zero_offsets = list(raw_flexions)
                notification_text = "ZERO TARE CALIBRATED"
                notification_until = time.time() + 2.5
                print(f"[CALIBRATION] Rest zero offsets set: {[round(x, 2) for x in zero_offsets]}")
            elif key == ord('r'):
                zero_offsets = [0.0, 0.0, 0.0, 0.0, 0.0]
                smoothed_angles = [0.0, 0.0, 0.0, 0.0, 0.0, 90.0]
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
