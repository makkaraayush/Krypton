#!/usr/bin/env python3
"""
Krypton Standalone Servo & Tendon Test Utility
Allows direct hardware angle verification over Serial USB or Wi-Fi UDP
without running the MediaPipe camera pipeline. Supports 6 channels (5 Fingers + Wrist).
Features interactive CLI and auto-detection of USB Serial port.
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


FINGER_NAMES = ["Thumb (CH0)", "Index (CH1)", "Middle (CH2)", "Ring (CH3)", "Pinky (CH4)", "Wrist (CH5)"]

PRESET_POSES = {
    "open": [0.0, 0.0, 0.0, 0.0, 0.0, 90.0],
    "fist": [180.0, 180.0, 180.0, 180.0, 180.0, 90.0],
    "point": [180.0, 0.0, 180.0, 180.0, 180.0, 90.0],
    "peace": [180.0, 0.0, 0.0, 180.0, 180.0, 90.0],
    "thumbs_up": [0.0, 180.0, 180.0, 180.0, 180.0, 90.0],
    "rock": [0.0, 0.0, 180.0, 180.0, 0.0, 90.0],
    "wrist_left": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    "wrist_center": [0.0, 0.0, 0.0, 0.0, 0.0, 90.0],
    "wrist_right": [0.0, 0.0, 0.0, 0.0, 0.0, 180.0],
}


def find_default_serial_port():
    if not HAS_SERIAL:
        return None
    ports = serial.tools.list_ports.comports()
    for p in ports:
        desc = p.description.lower()
        if any(x in desc for x in ["cp210", "ch340", "ftdi", "usb serial", "uart", "arduino", "esp32"]):
            return p.device
    if ports:
        return ports[0].device
    return None


def send_payload(payload: str, mode: str, ip: str, udp_port: int, ser_obj):
    if mode == "udp":
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(payload.encode('ascii'), (ip, udp_port))
        sock.close()
    elif mode == "serial" and ser_obj:
        ser_obj.write(payload.encode('ascii'))


def interactive_menu(mode: str, ip: str, udp_port: int, ser_obj):
    print("\n---------------- Interactive Mode ----------------")
    print(" 1 : Open Hand (0 deg / Wrist 90 deg)")
    print(" 2 : Fist (180 deg / Wrist 90 deg)")
    print(" 3 : Point (Index open)")
    print(" 4 : Peace (V)")
    print(" 5 : Thumbs Up")
    print(" 6 : Wrist Left (0 deg)")
    print(" 7 : Wrist Center (90 deg)")
    print(" 8 : Wrist Right (180 deg)")
    print(" 9 : Smooth Sweep (Fingers 0 -> 180 -> 0)")
    print(" c : Set Single Channel Angle (e.g. 'c 1 90')")
    print(" q : Quit")
    print("--------------------------------------------------")

    while True:
        try:
            cmd = input("Krypton> ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            break

        if not cmd:
            continue
        if cmd == "q" or cmd == "exit":
            break
        elif cmd == "1":
            angles = PRESET_POSES["open"]
            send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
            print("[*] Open Hand sent.")
        elif cmd == "2":
            angles = PRESET_POSES["fist"]
            send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
            print("[*] Fist sent.")
        elif cmd == "3":
            angles = PRESET_POSES["point"]
            send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
            print("[*] Point sent.")
        elif cmd == "4":
            angles = PRESET_POSES["peace"]
            send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
            print("[*] Peace sign sent.")
        elif cmd == "5":
            angles = PRESET_POSES["thumbs_up"]
            send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
            print("[*] Thumbs Up sent.")
        elif cmd == "6":
            angles = PRESET_POSES["wrist_left"]
            send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
            print("[*] Wrist Left (0 deg) sent.")
        elif cmd == "7":
            angles = PRESET_POSES["wrist_center"]
            send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
            print("[*] Wrist Center (90 deg) sent.")
        elif cmd == "8":
            angles = PRESET_POSES["wrist_right"]
            send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
            print("[*] Wrist Right (180 deg) sent.")
        elif cmd == "9":
            print("[*] Sweeping fingers 0 -> 180 -> 0...")
            for ang in range(0, 185, 5):
                send_payload(f"T:{ang:.1f},I:{ang:.1f},M:{ang:.1f},R:{ang:.1f},P:{ang:.1f},W:90.0\n", mode, ip, udp_port, ser_obj)
                time.sleep(0.04)
            time.sleep(0.3)
            for ang in range(180, -5, -5):
                send_payload(f"T:{ang:.1f},I:{ang:.1f},M:{ang:.1f},R:{ang:.1f},P:{ang:.1f},W:90.0\n", mode, ip, udp_port, ser_obj)
                time.sleep(0.04)
            print("[*] Sweep complete.")
        elif cmd.startswith("c"):
            parts = cmd.split()
            if len(parts) == 3:
                try:
                    ch = int(parts[1])
                    val = float(parts[2])
                    if 0 <= ch <= 5 and 0 <= val <= 180:
                        angles = [0.0, 0.0, 0.0, 0.0, 0.0, 90.0]
                        angles[ch] = val
                        send_payload(f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n", mode, ip, udp_port, ser_obj)
                        print(f"[*] Set CH {ch} ({FINGER_NAMES[ch]}) to {val} deg.")
                    else:
                        print("[!] Channel must be 0-5, angle must be 0-180.")
                except ValueError:
                    print("[!] Usage: c <channel 0-5> <angle 0-180>")
            else:
                print("[!] Usage: c <channel 0-5> <angle 0-180> (e.g. 'c 1 90')")
        else:
            print("[!] Unknown command. Enter 1-9, 'c <ch> <ang>', or 'q'.")


def main():
    parser = argparse.ArgumentParser(description="Krypton Servo Test CLI")
    parser.add_argument("--mode", choices=["serial", "udp", "auto"], default="auto", help="Communication mode (default: auto)")
    parser.add_argument("--ip", default="192.168.4.1", help="ESP32 IP address for UDP")
    parser.add_argument("--udp-port", type=int, default=8888, help="UDP port (default: 8888)")
    parser.add_argument("--port", default=None, help="Serial COM port (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--channel", type=int, choices=range(0, 6), default=None, help="Single channel to test (0:Thumb - 5:Wrist)")
    parser.add_argument("--angle", type=float, default=90.0, help="Target angle for single channel (0 to 180 deg)")
    parser.add_argument("--pose", choices=list(PRESET_POSES.keys()), default=None, help="Send preset pose")
    parser.add_argument("--sweep", action="store_true", help="Run 0-to-180 smooth test sweep across all fingers")
    parser.add_argument("--step", type=float, default=5.0, help="Sweep step degrees")
    args = parser.parse_args()

    effective_mode = args.mode
    ser_obj = None

    if effective_mode == "auto":
        target_port = args.port or find_default_serial_port()
        if target_port and HAS_SERIAL:
            effective_mode = "serial"
        else:
            effective_mode = "udp"

    if effective_mode == "serial":
        if not HAS_SERIAL:
            print("[ERROR] pyserial is required for serial mode. Install with: pip install pyserial")
            sys.exit(1)

        target_port = args.port or find_default_serial_port()
        if not target_port:
            print("[!] No USB COM port detected, falling back to UDP...")
            effective_mode = "udp"
        else:
            print(f"[*] Opening serial port: {target_port} @ {args.baud} baud")
            try:
                ser_obj = serial.Serial(target_port, args.baud, timeout=0.1)
                time.sleep(1.5)  # Allow ESP32 reboot window if DTR pulled low
            except Exception as e:
                print(f"[!] Serial error: {e}. Falling back to UDP...")
                effective_mode = "udp"
                ser_obj = None

    print("==================================================")
    print("           KRYPTON HARDWARE SERVO TEST            ")
    print("==================================================")
    print(f"[*] Telemetry Mode : {effective_mode.upper()}")
    if effective_mode == "udp":
        print(f"[*] UDP Target     : {args.ip}:{args.udp_port}")

    try:
        if args.pose:
            angles = PRESET_POSES[args.pose]
            payload = f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n"
            print(f"[*] Sending pose [{args.pose.upper()}]: {angles}")
            send_payload(payload, effective_mode, args.ip, args.udp_port, ser_obj)

        elif args.sweep:
            print("[*] Starting sweep: 0 -> 180 -> 0 degrees...")
            for ang in range(0, 185, int(args.step)):
                payload = f"T:{ang:.1f},I:{ang:.1f},M:{ang:.1f},R:{ang:.1f},P:{ang:.1f},W:90.0\n"
                send_payload(payload, effective_mode, args.ip, args.udp_port, ser_obj)
                time.sleep(0.04)
            time.sleep(0.5)
            for ang in range(180, -5, -int(args.step)):
                payload = f"T:{ang:.1f},I:{ang:.1f},M:{ang:.1f},R:{ang:.1f},P:{ang:.1f},W:90.0\n"
                send_payload(payload, effective_mode, args.ip, args.udp_port, ser_obj)
                time.sleep(0.04)
            print("[*] Sweep complete.")

        elif args.channel is not None:
            name = FINGER_NAMES[args.channel]
            angles = [0.0, 0.0, 0.0, 0.0, 0.0, 90.0]
            angles[args.channel] = max(0.0, min(args.angle, 180.0))
            payload = f"T:{angles[0]:.1f},I:{angles[1]:.1f},M:{angles[2]:.1f},R:{angles[3]:.1f},P:{angles[4]:.1f},W:{angles[5]:.1f}\n"
            print(f"[*] Setting {name} (CH {args.channel}) to {angles[args.channel]} deg")
            send_payload(payload, effective_mode, args.ip, args.udp_port, ser_obj)

        else:
            interactive_menu(effective_mode, args.ip, args.udp_port, ser_obj)

    finally:
        if ser_obj and ser_obj.is_open:
            ser_obj.close()


if __name__ == "__main__":
    main()
