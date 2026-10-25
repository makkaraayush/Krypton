"""
Krypton Teleoperation Communicator
Handles non-blocking data transmission over Serial USB, Wi-Fi UDP, or HTTP REST.
"""

import time
import socket
import threading
from typing import Optional, Tuple

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
    def __init__(self, mode: str = "udp", ip: str = "192.168.4.1", 
                 udp_port: int = 8888, serial_port: Optional[str] = None, 
                 baud: int = 115200):
        self.mode = mode.lower()  # "udp", "serial", "http", "none"
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

    def setup_connection(self):
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

            target_port = self.serial_port
            if not target_port:
                # Auto-detect CP2102, CH340, or FTDI USB bridge
                ports = serial.tools.list_ports.comports()
                for p in ports:
                    desc = p.description.lower()
                    if any(x in desc for x in ["cp210", "ch340", "ftdi", "usb serial", "uart"]):
                        target_port = p.device
                        break
                if not target_port and ports:
                    target_port = ports[0].device

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
        """Cycles between UDP, Serial, and None modes."""
        self.close()
        if self.mode == "udp":
            self.mode = "serial"
        elif self.mode == "serial":
            self.mode = "none"
        else:
            self.mode = "udp"
        self.setup_connection()
        return self.mode

    def send_angles(self, thumb: float, index: float, middle: float, ring: float, pinky: float):
        """
        Sends 5 finger angles (0 to 180 degrees) to ESP32.
        Payload format: T:45.0,I:90.0,M:120.0,R:120.0,P:120.0\n
        """
        now = time.time()
        if now - self._last_send_time < self._min_interval:
            return

        self._last_send_time = now
        payload = f"T:{thumb:.1f},I:{index:.1f},M:{middle:.1f},R:{ring:.1f},P:{pinky:.1f}\n"

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
