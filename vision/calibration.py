"""
Krypton Kinematics Calibration & Progressive Mapping Module
Handles non-linear piecewise curve mapping from human hand flexion (0.0 - 1.0)
to robotic servo angles (0 - 180 degrees), preventing tension spikes and matching
exact mechanical joint kinematics.
"""

import os
import json

PROFILE_PATH = os.path.join(os.path.dirname(__file__), "calibration_profile.json")

DEFAULT_PROFILE = {
    "thumb": {
        "min_angle": 0.0,
        "mid_angle": 85.0,
        "max_angle": 180.0,
        "exponent": 1.25
    },
    "index": {
        "min_angle": 0.0,
        "mid_angle": 85.0,
        "max_angle": 180.0,
        "exponent": 1.35
    },
    "middle": {
        "min_angle": 0.0,
        "mid_angle": 85.0,
        "max_angle": 180.0,
        "exponent": 1.30
    },
    "ring": {
        "min_angle": 0.0,
        "mid_angle": 80.0,
        "max_angle": 180.0,
        "exponent": 1.25
    },
    "pinky": {
        "min_angle": 0.0,
        "mid_angle": 80.0,
        "max_angle": 180.0,
        "exponent": 1.25
    },
    "wrist": {
        "min_angle": 0.0,
        "mid_angle": 90.0,
        "max_angle": 180.0,
        "exponent": 1.0
    }
}

CHANNEL_KEYS = ["thumb", "index", "middle", "ring", "pinky", "wrist"]


def load_profile(path: str = PROFILE_PATH) -> dict:
    """Loads calibration profile from JSON, or returns defaults if missing/invalid."""
    if os.path.exists(path):
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
                profile = {}
                for key in CHANNEL_KEYS:
                    if key in data and isinstance(data[key], dict):
                        profile[key] = {
                            "min_angle": float(data[key].get("min_angle", DEFAULT_PROFILE[key]["min_angle"])),
                            "mid_angle": float(data[key].get("mid_angle", DEFAULT_PROFILE[key]["mid_angle"])),
                            "max_angle": float(data[key].get("max_angle", DEFAULT_PROFILE[key]["max_angle"])),
                            "exponent": float(data[key].get("exponent", DEFAULT_PROFILE[key]["exponent"]))
                        }
                    else:
                        profile[key] = dict(DEFAULT_PROFILE[key])
                return profile
        except Exception as e:
            print(f"[CALIB] Warning: Could not parse {path} ({e}). Using default profile.")
    return {k: dict(v) for k, v in DEFAULT_PROFILE.items()}


def save_profile(profile: dict, path: str = PROFILE_PATH):
    """Saves calibration profile to JSON."""
    try:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(profile, f, indent=2)
        print(f"[CALIB] Saved calibration profile to {path}")
    except Exception as e:
        print(f"[CALIB] Error saving profile to {path}: {e}")


def map_flexion_to_angle(flexion: float, cfg: dict) -> float:
    """
    Progressively maps normalized flexion [0.0, 1.0] to servo angle [min_angle, max_angle].
    Uses 2-segment piecewise curve through mid_angle at 50% flexion with power curvature.
    """
    f = max(0.0, min(float(flexion), 1.0))
    min_a = float(cfg.get("min_angle", 0.0))
    mid_a = float(cfg.get("mid_angle", 90.0))
    max_a = float(cfg.get("max_angle", 180.0))
    exp = max(0.2, min(float(cfg.get("exponent", 1.0)), 3.0))

    if f <= 0.5:
        # First half: 0.0 to 0.5 -> min_a to mid_a
        t = f / 0.5
        t_curved = t ** exp
        angle = min_a + t_curved * (mid_a - min_a)
    else:
        # Second half: 0.5 to 1.0 -> mid_a to max_a
        t = (f - 0.5) / 0.5
        # Soften late tension spike with reciprocal curve
        t_curved = t ** (1.0 / max(exp * 0.8, 0.5))
        angle = mid_a + t_curved * (max_a - mid_a)

    return max(0.0, min(angle, 180.0))

