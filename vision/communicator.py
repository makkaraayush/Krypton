"""
Krypton Teleoperation Communicator
Handles non-blocking data transmission over Serial USB, Wi-Fi UDP, or HTTP REST.
Supports auto-detection of USB Serial and 6-channel hardware telemetry (5 fingers + wrist).
"""

import time
import socket
from typing import Optional

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False


class ArmCommunicator:
    def __init__(self, mode: str = "auto", ip: str = "192.168.4.1", 
                 udp_port: int = 8888, serial_port: Optional[str] = None, 
                 baud: int = 115200):
        self.requested_mode = mode.lower()  # "auto", "serial", "udp", "none"
        self.mode = self.requested_mode
        self.ip = ip
        self.udp_port = udp_port
        self.serial_port = serial_port
        self.baud = baud

        self._ser = None
        self._sock = None
        self._last_send_time = 0.0
        self._last_reconnect_attempt = 0.0
        self._min_interval = 1.0 / 50.0  # Cap transmission at 50Hz
        self.is_connected = False
        self.status_message = "Init"

        self.setup_connection()

    def _find_serial_port(self) -> Optional[str]:
        if self.serial_port:
            return self.serial_port
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

    def setup_connection(self):
        # Auto-detect mode: checks USB Serial first; if unavailable, falls back to UDP Wi-Fi
        if self.requested_mode == "auto":
            target_port = self._find_serial_port()
            if target_port and HAS_SERIAL:
                try:
                    self._ser = serial.Serial(target_port, self.baud, timeout=0.05)
                    self.mode = "serial"
                    self.is_connected = True
                    self.status_message = f"Auto -> Serial {target_port} @ {self.baud}"
                    return
                except Exception:
                    self._ser = None

            # Fall back to UDP Wi-Fi
            try:
                self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self._sock.setblocking(False)
                self.mode = "udp"
                self.is_connected = True
                self.status_message = f"Auto -> UDP {self.ip}:{self.udp_port}"
                return
            except Exception as e:
                self.mode = "none"
                self.is_connected = False
                self.status_message = f"Auto -> Offline ({e})"
                return

        if self.mode == "udp":
            try:
                self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self._sock.setblocking(False)
                self.is_connected = True
                self.status_message = f"UDP {self.ip}:{self.udp_port}"
            except Exception as e:
                self.is_connected = False
                self.status_message = f"UDP Err: {e}"

        elif self.mode == "serial":
            if not HAS_SERIAL:
                self.is_connected = False
                self.status_message = "pyserial not installed"
                return

            target_port = self._find_serial_port()
            if target_port:
                try:
                    self._ser = serial.Serial(target_port, self.baud, timeout=0.05)
                    self.is_connected = True
                    self.status_message = f"Serial {target_port} @ {self.baud}"
                except Exception as e:
                    self.is_connected = False
                    self.status_message = f"Serial Err: {e}"
            else:
                self.is_connected = False
                self.status_message = "No COM port found"

        elif self.mode == "none":
            self.is_connected = True
            self.status_message = "Telemetry Off (Visual Only)"

    def cycle_mode(self):
        """Cycles between Auto, Serial, UDP, and None modes."""
        self.close()
        if self.requested_mode == "auto":
            self.requested_mode = "serial"
        elif self.requested_mode == "serial":
            self.requested_mode = "udp"
        elif self.requested_mode == "udp":
            self.requested_mode = "none"
        else:
            self.requested_mode = "auto"
        self.mode = self.requested_mode
        self.setup_connection()
        return self.mode

    def send_angles(self, thumb: float, index: float, middle: float, ring: float, pinky: float, wrist: float = 90.0):
        """
        Sends 6 servo angles (0 to 180 degrees) to ESP32.
        Payload format: T:45.0,I:90.0,M:120.0,R:120.0,P:120.0,W:90.0\n
        """
        now = time.time()
        if now - self._last_send_time < self._min_interval:
            return

        self._last_send_time = now
        payload = f"T:{thumb:.1f},I:{index:.1f},M:{middle:.1f},R:{ring:.1f},P:{pinky:.1f},W:{wrist:.1f}\n"

        if self.mode == "udp" and self._sock:
            try:
                self._sock.sendto(payload.encode('ascii'), (self.ip, self.udp_port))
            except Exception:
                pass

        elif self.mode == "serial":
            if not self.is_connected or not self._ser or not self._ser.is_open:
                if now - self._last_reconnect_attempt > 2.0:
                    self._last_reconnect_attempt = now
                    self.setup_connection()

            if self._ser and self._ser.is_open:
                try:
                    self._ser.write(payload.encode('ascii'))
                except Exception:
                    self.is_connected = False
                    self.status_message = "Serial Disconnected"
                    try:
                        self._ser.close()
                    except Exception:
                        pass
                    self._ser = None

    def close(self):
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass
            self._sock = None

        if self._ser:
            try:
                self._ser.close()
            except Exception:
                pass
            self._ser = None
        self.is_connected = False
