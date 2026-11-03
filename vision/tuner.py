"""
Krypton Interactive Calibration & Kinematic Tuner
Provides real-time slider controls and multi-point position matching so users can
precisely align human hand articulation with robotic tendon actuation curves.
Saves calibrated progressive profiles directly to calibration_profile.json.
"""

import sys
import os
import time
import threading
import tkinter as tk
from tkinter import ttk, messagebox
import cv2
import numpy as np

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
import calibration
from hand_tracker import calculate_angle_3d, compute_finger_flexions, compute_wrist_roll


class KryptonTunerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Krypton Bionic Arm - Progressive Kinematics Tuner")
        self.root.geometry("820x760")
        self.root.configure(bg="#0d1117")

        # Load existing calibration or defaults
        self.profile = calibration.load_profile()
        self.channel_keys = calibration.CHANNEL_KEYS
        self.channel_labels = ["Thumb (CH0)", "Index (CH1)", "Middle (CH2)", "Ring (CH3)", "Pinky (CH4)", "Wrist (CH5)"]

        # Hardware connection
        self.comm = ArmCommunicator(mode="auto")
        
        # State
        self.running = True
        self.live_human_flexions = [0.0] * 6
        self.live_servo_angles = [0.0] * 6
        self.live_servo_angles[5] = 90.0

        # Build UI
        self._build_ui()

        # Start Camera Thread
        self.cam_thread = threading.Thread(target=self._camera_loop, daemon=True)
        self.cam_thread.start()

        # Periodic UI update
        self.root.after(30, self._update_ui_loop)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TLabel", background="#0d1117", foreground="#e6edf3", font=("Segoe UI", 9))
        style.configure("Header.TLabel", font=("Segoe UI", 12, "bold"), foreground="#00f0ff")
        style.configure("Channel.TLabel", font=("Segoe UI", 10, "bold"), foreground="#ffb700")
        style.configure("TButton", font=("Segoe UI", 9, "bold"), padding=5)

        # Header Frame
        hdr_frame = tk.Frame(self.root, bg="#161b22", relief="ridge", bd=1)
        hdr_frame.pack(fill="x", padx=12, pady=8)
        
        lbl_title = tk.Label(hdr_frame, text="KRYPTON // PROGRESSIVE CURVE TUNER", font=("Segoe UI", 13, "bold"), bg="#161b22", fg="#00f0ff")
        lbl_title.pack(side="left", padx=12, pady=8)

        self.lbl_comm_status = tk.Label(hdr_frame, text=f"Status: {self.comm.status_message}", font=("Segoe UI", 9), bg="#161b22", fg="#2ed573")
        self.lbl_comm_status.pack(side="right", padx=12, pady=8)

        # Instructions banner
        instr = tk.Label(self.root, text="Fold your human finger to any desired angle -> Adjust sliders until robotic arm matches -> Click 'Capture' -> 'Save Profile'.",
                         font=("Segoe UI", 9), bg="#0d1117", fg="#8b949e")
        instr.pack(pady=2)

        # Main notebook/frame for finger channels
        main_frame = tk.Frame(self.root, bg="#0d1117")
        main_frame.pack(fill="both", expand=True, padx=12, pady=6)

        self.slider_vars = {}
        self.telemetry_labels = {}

        for idx, key in enumerate(self.channel_keys):
            cfg = self.profile[key]
            card = tk.LabelFrame(main_frame, text=f" {self.channel_labels[idx]} ", bg="#161b22", fg="#00f0ff", font=("Segoe UI", 10, "bold"), bd=1)
            card.grid(row=idx // 2, column=idx % 2, padx=8, pady=6, sticky="nsew")
            main_frame.grid_columnconfigure(idx % 2, weight=1)

            # Live reading row
            row_tel = tk.Frame(card, bg="#161b22")
            row_tel.pack(fill="x", padx=8, pady=2)

            t_lbl = tk.Label(row_tel, text="Human: 0%  |  Arm: 0°", font=("Segoe UI", 9, "bold"), bg="#161b22", fg="#e6edf3")
            t_lbl.pack(side="left")
            self.telemetry_labels[key] = t_lbl

            btn_match = tk.Button(row_tel, text="Capture Point", bg="#238636", fg="#ffffff", font=("Segoe UI", 8, "bold"),
                                  activebackground="#2ea043", relief="flat", command=lambda k=key, i=idx: self._capture_match(k, i))
            btn_match.pack(side="right")

            # Sliders grid
            grid_s = tk.Frame(card, bg="#161b22")
            grid_s.pack(fill="x", padx=8, pady=4)

            # Min Angle
            tk.Label(grid_s, text="Min (Open):", bg="#161b22", fg="#8b949e").grid(row=0, column=0, sticky="w")
            var_min = tk.DoubleVar(value=cfg["min_angle"])
            s_min = tk.Scale(grid_s, from_=0, to=60, orient="horizontal", variable=var_min, bg="#161b22", fg="#00f0ff",
                             highlightthickness=0, length=140, command=lambda v, k=key: self._on_slider_change(k))
            s_min.grid(row=0, column=1, padx=4)

            # Mid Angle (50% flexion point)
            tk.Label(grid_s, text="Mid (Half Clench):", bg="#161b22", fg="#8b949e").grid(row=1, column=0, sticky="w")
            var_mid = tk.DoubleVar(value=cfg["mid_angle"])
            s_mid = tk.Scale(grid_s, from_=30, to=150, orient="horizontal", variable=var_mid, bg="#161b22", fg="#00f0ff",
                             highlightthickness=0, length=140, command=lambda v, k=key: self._on_slider_change(k))
            s_mid.grid(row=1, column=1, padx=4)

            # Max Angle
            tk.Label(grid_s, text="Max (Full Clench):", bg="#161b22", fg="#8b949e").grid(row=2, column=0, sticky="w")
            var_max = tk.DoubleVar(value=cfg["max_angle"])
            s_max = tk.Scale(grid_s, from_=100, to=180, orient="horizontal", variable=var_max, bg="#161b22", fg="#00f0ff",
                             highlightthickness=0, length=140, command=lambda v, k=key: self._on_slider_change(k))
            s_max.grid(row=2, column=1, padx=4)

            # Curve Exponent
            tk.Label(grid_s, text="Curve Progression:", bg="#161b22", fg="#8b949e").grid(row=3, column=0, sticky="w")
            var_exp = tk.DoubleVar(value=cfg["exponent"])
            s_exp = tk.Scale(grid_s, from_=0.5, to=2.5, resolution=0.05, orient="horizontal", variable=var_exp, bg="#161b22", fg="#ffb700",
                             highlightthickness=0, length=140, command=lambda v, k=key: self._on_slider_change(k))
            s_exp.grid(row=3, column=1, padx=4)

            self.slider_vars[key] = {
                "min": var_min,
                "mid": var_mid,
                "max": var_max,
                "exp": var_exp
            }

        # Footer Actions
        ftr_frame = tk.Frame(self.root, bg="#161b22", relief="ridge", bd=1)
        ftr_frame.pack(fill="x", padx=12, pady=10)

        btn_save = tk.Button(ftr_frame, text="SAVE & APPLY PROFILE", bg="#1f6feb", fg="#ffffff", font=("Segoe UI", 10, "bold"),
                             activebackground="#388bfd", relief="flat", padx=14, pady=6, command=self._save_profile)
        btn_save.pack(side="left", padx=10, pady=8)

        btn_sweep = tk.Button(ftr_frame, text="TEST PROGRESSIVE SWEEP", bg="#238636", fg="#ffffff", font=("Segoe UI", 10, "bold"),
                              activebackground="#2ea043", relief="flat", padx=14, pady=6, command=self._test_sweep)
        btn_sweep.pack(side="left", padx=6, pady=8)

        btn_reset = tk.Button(ftr_frame, text="Reset Defaults", bg="#30363d", fg="#e6edf3", font=("Segoe UI", 9),
                              activebackground="#484f58", relief="flat", padx=10, pady=6, command=self._reset_defaults)
        btn_reset.pack(side="right", padx=10, pady=8)

    def _on_slider_change(self, key):
        """Update live configuration dictionary when sliders are moved."""
        v = self.slider_vars[key]
        self.profile[key]["min_angle"] = v["min"].get()
        self.profile[key]["mid_angle"] = v["mid"].get()
        self.profile[key]["max_angle"] = v["max"].get()
        self.profile[key]["exponent"] = v["exp"].get()

    def _capture_match(self, key, idx):
        """Matches the current human finger position to the arm setting."""
        raw_f = self.live_human_flexions[idx]
        current_deg = self.live_servo_angles[idx]

        if raw_f <= 0.25:
            self.slider_vars[key]["min"].set(round(current_deg, 1))
            msg = f"Captured MIN open angle for {key.upper()} at {current_deg:.1f}°"
        elif raw_f >= 0.75:
            self.slider_vars[key]["max"].set(round(current_deg, 1))
            msg = f"Captured MAX clench angle for {key.upper()} at {current_deg:.1f}°"
        else:
            self.slider_vars[key]["mid"].set(round(current_deg, 1))
            msg = f"Captured MID grasp point for {key.upper()} at {current_deg:.1f}° (Human: {int(raw_f * 100)}%)"

        self._on_slider_change(key)
        messagebox.showinfo("Position Calibrated", msg)

    def _save_profile(self):
        for k in self.channel_keys:
            self._on_slider_change(k)
        calibration.save_profile(self.profile)
        messagebox.showinfo("Success", "Calibration profile saved!\nHand tracker will now use these exact progressive curves.")

    def _reset_defaults(self):
        if messagebox.askyesno("Reset Defaults", "Reset all curves to factory defaults?"):
            self.profile = {k: dict(v) for k, v in calibration.DEFAULT_PROFILE.items()}
            for k in self.channel_keys:
                cfg = self.profile[k]
                self.slider_vars[k]["min"].set(cfg["min_angle"])
                self.slider_vars[k]["mid"].set(cfg["mid_angle"])
                self.slider_vars[k]["max"].set(cfg["max_angle"])
                self.slider_vars[k]["exp"].set(cfg["exponent"])
            calibration.save_profile(self.profile)

    def _test_sweep(self):
        def sweep_job():
            print("[SWEEP] Testing progressive curve sweep 0 -> 180 -> 0...")
            for f in np.linspace(0.0, 1.0, 30):
                angs = [calibration.map_flexion_to_angle(f, self.profile[k]) for k in self.channel_keys]
                self.comm.send_angles(angs[0], angs[1], angs[2], angs[3], angs[4], angs[5])
                time.sleep(0.04)
            time.sleep(0.3)
            for f in np.linspace(1.0, 0.0, 30):
                angs = [calibration.map_flexion_to_angle(f, self.profile[k]) for k in self.channel_keys]
                self.comm.send_angles(angs[0], angs[1], angs[2], angs[3], angs[4], angs[5])
                time.sleep(0.04)
            print("[SWEEP] Sweep test complete.")
        threading.Thread(target=sweep_job, daemon=True).start()

    def _camera_loop(self):
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            print("[TUNER] Could not open camera 0.")
            return

        mp_hands = mp.solutions.hands
        hands = mp_hands.Hands(
            static_image_mode=False,
            max_num_hands=1,
            min_detection_confidence=0.7,
            min_tracking_confidence=0.6
        )
        mp_draw = mp.solutions.drawing_utils
        mp_draw_styles = mp.solutions.drawing_styles

        while self.running:
            ret, frame = cap.read()
            if not ret:
                time.sleep(0.03)
                continue

            frame = cv2.flip(frame, 1)
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            results = hands.process(rgb)

            if results.multi_hand_landmarks:
                for hand_landmarks in results.multi_hand_landmarks:
                    mp_draw.draw_landmarks(
                        frame,
                        hand_landmarks,
                        mp_hands.HAND_CONNECTIONS,
                        mp_draw_styles.get_default_hand_landmarks_style(),
                        mp_draw_styles.get_default_hand_connections_style()
                    )

                    # Compute raw flexions
                    raw_flex = compute_finger_flexions(hand_landmarks.landmark)
                    for i in range(5):
                        self.live_human_flexions[i] = raw_flex[i]
                        # Compute current angle from active profile
                        k = self.channel_keys[i]
                        self.live_servo_angles[i] = calibration.map_flexion_to_angle(raw_flex[i], self.profile[k])

                    # Wrist roll
                    w_deg = compute_wrist_roll(hand_landmarks.landmark)
                    self.live_human_flexions[5] = w_deg / 180.0
                    self.live_servo_angles[5] = w_deg

                    # Stream live angles to arm so user sees arm move in real time
                    self.comm.send_angles(
                        self.live_servo_angles[0],
                        self.live_servo_angles[1],
                        self.live_servo_angles[2],
                        self.live_servo_angles[3],
                        self.live_servo_angles[4],
                        self.live_servo_angles[5]
                    )

            # Draw HUD on preview window
            cv2.putText(frame, "KRYPTON TUNER // REAL-TIME CALIBRATION", (15, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 240, 0), 2, cv2.LINE_AA)
            cv2.putText(frame, "Adjust curves in Tuner window -> Click 'Save & Apply'", (15, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, (200, 200, 200), 1, cv2.LINE_AA)

            cv2.imshow("Krypton Kinematics Preview", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

        cap.release()
        cv2.destroyAllWindows()

    def _update_ui_loop(self):
        if not self.running:
            return
        
        # Update live telemetry labels
        for idx, k in enumerate(self.channel_keys):
            pct = int(self.live_human_flexions[idx] * 100)
            ang = int(self.live_servo_angles[idx])
            lbl = self.telemetry_labels.get(k)
            if lbl:
                lbl.config(text=f"Human: {pct:3d}%  |  Arm Servo: {ang:3d}°")

        self.lbl_comm_status.config(text=f"Status: {self.comm.status_message}")
        self.root.after(50, self._update_ui_loop)

    def _on_close(self):
        self.running = False
        self.comm.close()
        self.root.destroy()


def main():
    root = tk.Tk()
    app = KryptonTunerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()

