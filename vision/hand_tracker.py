# MediaPipe Hand Tracking with Angle & Distance Flexion Math
import sys
import cv2
import numpy as np

try:
    import mediapipe as mp
except ImportError:
    print("Run: pip install -r requirements.txt")
    sys.exit(1)

def calculate_flexions(landmarks):
    # Normalized flexion calculations
    wrist = landmarks[0]
    middle_mcp = landmarks[9]
    span = np.linalg.norm([wrist.x - middle_mcp.x, wrist.y - middle_mcp.y])
    flexions = []
    # 5 fingers
    for tip in [4, 8, 12, 16, 20]:
        dist = np.linalg.norm([landmarks[tip].x - wrist.x, landmarks[tip].y - wrist.y]) / span
        flexions.append(np.clip((1.8 - dist) / 1.0, 0.0, 1.0))
    return flexions
