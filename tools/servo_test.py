#!/usr/bin/env python3
"""
Krypton Standalone Servo & Tendon Test Utility
Allows direct hardware angle verification over Serial USB or Wi-Fi UDP
without running the MediaPipe camera pipeline.
"""

import sys
import time
import argparse
import socket

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False


FINGER_NAMES = ["Thumb", "Index", "Middle", "Ring", "Pinky"]

PRESET_POSES = {
    "open": [0.0, 0.0, 0.0, 0.0, 0.0],
    "fist": [180.0, 180.0, 180.0, 180.0, 180.0],
    "point": [180.0, 0.0, 180.0, 180.0, 180.0],
    "peace": [180.0, 0.0, 0.0, 180.0, 180.0],
    "thumbs_up": [0.0, 180.0, 180.0, 180.0, 180.0],
    "rock": [0.0, 0.0, 180.0, 180.0, 0.0]
}


def send_payload(payload: str, mode: str, ip: str, udp_port: int, ser_obj):
    if mode == "udp":
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(payload.encode('ascii'), (ip, udp_port))
        sock.close()
    elif mode == "serial" and ser_obj:
        ser_obj.write(payload.encode('ascii'))


def main():
    parser = argparse.ArgumentParser(description="Krypton Servo Test CLI")
    parser.add_argument("--mode", choices=["serial", "udp"], default="udp", help="Communication mode")
    parser.add_argument("--ip", default="192.168.4.1", help="ESP32 IP address for UDP")
    parser.add_argument("--udp-port", type=int, default=8888, help="UDP port (default: 8888)")
    parser.add_argument("--port", default=None, help="Serial COM port (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--channel", type=int, choices=range(0, 5), default=None, help="Single channel to test (0:Thumb - 4:Pinky)")
    parser.add_argument("--angle", type=float, default=90.0, help="Target angle for single channel (0 to 180 deg)")
    parser.add_argument("--pose", choices=list(PRESET_POSES.keys()), default=None, help="Send preset pose")
    parser.add_argument("--sweep", action="store_true", help="Run 0-to-180 smooth test sweep across all fingers")
    parser.add_argument("--step", type=float, default=5.0, help="Sweep step degrees")
    args = parser.parse_args()

    ser_obj = None
    if args.mode == "serial":
        if not HAS_SERIAL:
            print("[ERROR] pyserial is required for serial mode. Install with: pip install pyserial")
            sys.exit(1)

        target_port = args.port
        if not target_port:
            ports = serial.tools.list_ports.comports()
            for p in ports:
                desc = p.description.lower()
                if any(x in desc for x in ["cp210", "ch340", "ftdi", "usb serial"]):
                    target_port = p.device
                    break
            if not target_port and ports:
                target_port = ports[0].device

        if not target_port:
            print("[ERROR] No serial port detected.")
            sys.exit(1)

        print(f"[*] Opening serial port: {target_port} @ {args.baud} baud")
        ser_obj = serial.Serial(target_port, args.baud, timeout=0.1)
        time.sleep(2.0)  # Allow ESP32 reboot window if DTR pulled low

    print("==================================================")
    print("           KRYPTON HARDWARE SERVO TEST            ")
    print("==================================================")
    print(f"[*] Mode: {args.mode.upper()}")

    try:
        if args.pose:
            angles = PRESET_POSES[args.pose]
            payload = f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f}\n"
            print(f"[*] Sending pose [{args.pose.upper()}]: {angles}")
            send_payload(payload, args.mode, args.ip, args.udp_port, ser_obj)

        elif args.sweep:
            print("[*] Starting sweep: 0 -> 180 -> 0 degrees...")
            for ang in range(0, 185, int(args.step)):
                payload = f"T:{ang:.1f},I:{ang:.1f},M:{ang:.1f},R:{ang:.1f},P:{ang:.1f}\n"
                send_payload(payload, args.mode, args.ip, args.udp_port, ser_obj)
                time.sleep(0.04)
            time.sleep(0.5)
            for ang in range(180, -5, -int(args.step)):
                payload = f"T:{ang:.1f},I:{ang:.1f},M:{ang:.1f},R:{ang:.1f},P:{ang:.1f}\n"
                send_payload(payload, args.mode, args.ip, args.udp_port, ser_obj)
                time.sleep(0.04)
            print("[*] Sweep complete.")

        elif args.channel is not None:
            name = FINGER_NAMES[args.channel]
            angles = [0.0] * 5
            angles[args.channel] = max(0.0, min(args.angle, 180.0))
            payload = f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f}\n"
            print(f"[*] Setting {name} (CH {args.channel}) to {angles[args.channel]} deg")
            send_payload(payload, args.mode, args.ip, args.udp_port, ser_obj)

        else:
            print("[!] Please specify --channel, --pose, or --sweep. Run with --help for details.")

    finally:
        if ser_obj and ser_obj.is_open:
            ser_obj.close()


if __name__ == "__main__":
    main()
