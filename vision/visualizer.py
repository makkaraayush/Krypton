"""
Cyberpunk Visual HUD for Krypton
Renders hand skeleton overlays, telemetry progress bars, FPS, and status indicators.
"""

import cv2
import numpy as np

# Cyberpunk Color Palette (BGR)
COLOR_BG = (16, 24, 32)
COLOR_CYAN = (255, 240, 0)       # Accent
COLOR_AMBER = (0, 183, 255)      # Highlights
COLOR_DARK = (24, 18, 12)
COLOR_TEXT = (240, 240, 240)
COLOR_BAR_BG = (40, 48, 56)
COLOR_SUCCESS = (115, 213, 46)
COLOR_WARN = (71, 71, 255)


class CyberHud:
    def __init__(self):
        self.finger_names = ["THUMB", "INDEX", "MIDDLE", "RING", "PINKY"]
        self.finger_keys = ["T", "I", "M", "R", "P"]

    def draw_telemetry_bars(self, frame, angles, flexion_pcts):
        """
        Renders 5 vertical tension gauges on the right side of the screen.
        """
        h, w, _ = frame.shape
        panel_w = 200
        panel_h = 240
        x_start = w - panel_w - 20
        y_start = 20

        # Semi-transparent backing panel
        overlay = frame.copy()
        cv2.rectangle(overlay, (x_start, y_start), (x_start + panel_w, y_start + panel_h), COLOR_DARK, -1)
        cv2.addWeighted(overlay, 0.75, frame, 0.25, 0, frame)
        cv2.rectangle(frame, (x_start, y_start), (x_start + panel_w, y_start + panel_h), COLOR_CYAN, 1)

        # Header
        cv2.putText(frame, "TENSION GAUGES", (x_start + 12, y_start + 24),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, COLOR_CYAN, 1, cv2.LINE_AA)

        # Render each finger bar
        bar_x = x_start + 14
        bar_w = 24
        max_bar_h = 130
        y_base = y_start + 195

        for i in range(5):
            pct = flexion_pcts[i]
            ang = angles[i]
            bx = bar_x + (i * 36)
            
            # Background track
            cv2.rectangle(frame, (bx, y_base - max_bar_h), (bx + bar_w, y_base), COLOR_BAR_BG, -1)

            # Active fill
            fill_h = int(max_bar_h * pct)
            color = COLOR_AMBER if pct > 0.8 else COLOR_CYAN
            if fill_h > 0:
                cv2.rectangle(frame, (bx, y_base - fill_h), (bx + bar_w, y_base), color, -1)

            # Border
            cv2.rectangle(frame, (bx, y_base - max_bar_h), (bx + bar_w, y_base), (60, 70, 80), 1)

            # Key label
            cv2.putText(frame, self.finger_keys[i], (bx + 6, y_base + 16),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, COLOR_TEXT, 1, cv2.LINE_AA)
            
            # Angle value
            cv2.putText(frame, f"{int(ang)}", (bx, y_base - max_bar_h - 6),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.35, COLOR_CYAN, 1, cv2.LINE_AA)

    def draw_status_bar(self, frame, fps, status_msg, mode, notification=""):
        """Renders top header badge with mode, FPS, and connection details."""
        h, w, _ = frame.shape
        
        # Top-left info box
        box_h = 95 if notification else 75
        cv2.rectangle(frame, (20, 20), (340, 20 + box_h), COLOR_DARK, -1)
        cv2.rectangle(frame, (20, 20), (340, 20 + box_h), (50, 60, 75), 1)

        cv2.putText(frame, "KRYPTON // VISION LINK", (32, 42),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, COLOR_CYAN, 1, cv2.LINE_AA)
        
        mode_text = f"MODE: {mode.upper()} | {status_msg}"
        cv2.putText(frame, mode_text, (32, 62),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, COLOR_TEXT, 1, cv2.LINE_AA)

        # FPS badge
        cv2.putText(frame, f"{int(fps)} FPS", (280, 42),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.45, COLOR_SUCCESS, 1, cv2.LINE_AA)

        # Notification banner (e.g. tare confirmation)
        if notification:
            cv2.putText(frame, f">> {notification}", (32, 85),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, COLOR_AMBER, 1, cv2.LINE_AA)

        # Footer hotkeys guide
        guide = "[Q] Quit  |  [M] Toggle Mode  |  [C] Zero Tare (Rest)  |  [R] Reset"
        cv2.putText(frame, guide, (20, h - 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.45, (180, 190, 200), 1, cv2.LINE_AA)
