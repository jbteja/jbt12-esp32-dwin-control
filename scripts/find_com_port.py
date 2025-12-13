"""
Find ESP32 COM ports helper

About:
- Scans available serial/COM ports and attempts to detect ESP32 devices
  based on common USB-Vendor and Product IDs.
- Useful when connecting an ESP32 dev board to a PC to locate the COM port.

Notes:
- On Windows, ports will be returned as "COMx". On Unix-like systems, they'll
  appear like "/dev/ttyUSB0" or "/dev/ttyACM0".
"""

__author__ = "Bhanu Teja J"
__version__ = "0.0.3"
__created__ = "2025-08-18"
__updated__ = "2025-12-12"

import serial.tools.list_ports

"""Common ESP32 USB Vendor/Product IDs"""
ESP32_IDS = [
    ("10C4", "EA60"),  # CP210x USB to UART Bridge
    ("1A86", "7523"),  # CH340 USB to Serial
    ("0403", "6001"),  # FTDI FT232
]

def is_esp32(port_info):
    """Check if the given port corresponds to known USB-Vendor"""
    vid = f"{port_info.vid:04X}" if port_info.vid else None
    pid = f"{port_info.pid:04X}" if port_info.pid else None
    return (vid, pid) in ESP32_IDS

def find_esp32_ports():
    """Find and return a list of COM ports connected to ESP32 devices"""
    ports = serial.tools.list_ports.comports()
    esp32_ports = [p.device for p in ports if is_esp32(p)]
    return esp32_ports

if __name__ == "__main__":
    esp32_ports = find_esp32_ports()

    # Display results
    if esp32_ports:
        print("Detected ESP32 on the following port(s):")
        for port in esp32_ports:
            print(f" - {port}")

    else:
        print("No ESP32 device detected.")
