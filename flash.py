#!/usr/bin/env python3
"""
This script helps in flashing production firmware onto ESP32 devices via serial port.

Usage:
    python flash.py [--list] [--port PORT] [--get-mac]

Notes:
- On Windows, ports will be returned as "COMx". On Unix-like systems, they'll
  appear like "/dev/ttyUSB0" or "/dev/ttyACM0".
"""

__author__ = "Bhanu Teja"
__version__ = "0.0.5"
__created__ = "2025-Aug-18"
__updated__ = "2026-Jan-02"

import os
import re
import sys
import json
import time
import serial
import argparse
import subprocess
import serial.tools.list_ports

try:
    import esptool
except ImportError:
    print("Error: The 'esptool' Python package is not installed.\nInstall it with: pip install esptool")
    sys.exit(1)

# -------------------------------
# CONFIGURATION
# -------------------------------

# Base paths
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

# Firmware types
FIRMWARE_TYPES = {
    "production": {
        "path": os.path.join(ROOT_DIR, "releases", "firmware"),
        "desc": "Final production firmware",
        "files": [
            ("bootloader.bin", "0x1000"),
            ("partitions.bin", "0x8000"),
            ("firmware.bin", "0x10000")
        ]
    }
}

# USB IDs of common boards
USB_IDS = [
    ("10C4", "EA60"),  # CP210x
    ("1A86", "7523"),  # CH340
    ("0403", "6001"),  # FTDI
]

# Type and flash settings
ESP_CHIP = "esp32"
FLASH_BAUD = "921600"

# Serial communication
SERIAL_BAUD = 115200
SERIAL_TIMEOUT = 3
RESET_DELAY = 2
MAC_READ_RETRIES = 3

# Commands for provisioning firmware
GET_MAC_CMD = "GET_MAC\n"

# -------------------------------
# Utility functions
# -------------------------------
def find_esp_port(port=None):
    """Find ESP32 serial port automatically or use specified port"""
    if port:
        # Check if specified port exists
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if port in ports:
            return port
        
        # Specified port not found
        return None
    
    # Auto-detect
    ports = serial.tools.list_ports.comports()
    for port in ports:
        vid = f"{port.vid:04X}" if port.vid else None
        pid = f"{port.pid:04X}" if port.pid else None

        if (vid, pid) in USB_IDS:
            return port.device
    
    # No ESP device found
    return None

def list_available_ports():
    """List all available serial ports"""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("[LIST] No serial ports found!")
        return
    
    print("Available ports:")
    for port in ports:
        vid = f"{port.vid:04X}" if port.vid else "N/A"
        pid = f"{port.pid:04X}" if port.pid else "N/A"
        is_esp = (vid, pid) in USB_IDS
        esp_mark = " [ESP]" if is_esp else ""
        print(f"  {port.device}: {port.description or 'N/A'} (VID:PID={vid}:{pid}){esp_mark}")

def reboot_device(port):
    """Restart ESP32 device via serial DTR/RTS toggling"""
    print(f"[PORT] Restarting device on {port}")
    try:
        with serial.Serial(port, baudrate=SERIAL_BAUD, timeout=SERIAL_TIMEOUT) as ser:
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.dtr = True
            ser.rts = False
            time.sleep(0.1)
        # print(f"[PORT] Device restarted successfully")
        return True

    except serial.SerialException as e:
        print(f"[PORT] Failed to restart device: {e}")
        return False

def flash_firmware(port, firmware_type="provision"):
    """Flash firmware to ESP32"""
    if firmware_type not in FIRMWARE_TYPES:
        print(f"[FLASH] Unknown firmware type: {firmware_type}")
        return False
    
    fw_config = FIRMWARE_TYPES[firmware_type]
    fw_path = fw_config["path"]
    
    # Check if firmware files exist
    for filename, _ in fw_config["files"]:
        filepath = os.path.join(fw_path, filename)
        if not os.path.exists(filepath):
            print(f"[FLASH] Firmware file not found: {filepath}")
            return False
    
    print("\n[IMPORTANT] Press and hold the BOOT button for 3 seconds, then release it\n")

    time.sleep(3)
    
    # Build esptool command
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", ESP_CHIP,
        "--port", port,
        "--baud", FLASH_BAUD,
        "write-flash", 
        "-z",
    ]
    
    # Add flash arguments (address, filename)
    for filename, address in fw_config["files"]:
        filepath = os.path.join(fw_path, filename)
        cmd.extend([address, filepath])
    
    try:
        subprocess.run(
            cmd,
            check=True,
            # capture_output=True
        )
        print(
            f"[FLASH] {firmware_type.capitalize()} firmware flashed successfully")
        return True

    except subprocess.CalledProcessError as e:
        print(f"[FLASH] Failed to flash firmware: {e}")
        return False

def read_mac(port):
    """Read MAC address via UART with retries"""
    print(f"[MAC] Fetching address from {port}")

    for attempt in range(MAC_READ_RETRIES):
        try:
            with serial.Serial(port, baudrate=SERIAL_BAUD, timeout=SERIAL_TIMEOUT) as ser:
                time.sleep(RESET_DELAY)
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                
                # Clear any pending data
                if ser.in_waiting:
                    ser.read(ser.in_waiting)
                
                # Send command twice for good measure
                ser.write(GET_MAC_CMD.encode())
                ser.flush()
                time.sleep(0.3)
                ser.write(GET_MAC_CMD.encode())
                ser.flush()

                MAC_REGEX = re.compile(
                    r'\bMAC:\s*([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})'
                )
                
                # Read response
                start_time = time.time()
                while (time.time() - start_time) < SERIAL_TIMEOUT:
                    if ser.in_waiting:
                        line = ser.readline().decode(errors="ignore").strip()    
                        if not line:
                            continue

                        match = MAC_REGEX.search(line)
                        if match:
                            mac = match.group(1)
                            print(f"[MAC] Address: {mac}")
                            return mac

                    time.sleep(0.01)
                
                # If we get here, no MAC found in this attempt
                print(f"[MAC] Timeout: No response (attempt {attempt + 1})")
                
        except serial.SerialException as e:
            print(f"[MAC] Serial error (attempt {attempt + 1}): {e}")

        # Restart device before next attempt
        if attempt == 0:
            reboot_device(port)
        
        # Wait before retrying (except on last attempt)
        if attempt < (MAC_READ_RETRIES - 1):
            time.sleep(0.5)
    
    print(f"[MAC] Failed to read address after {MAC_READ_RETRIES} attempts!")
    return None

# -------------------------------
# Main function
# -------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Release production provisioning tool"
    )
    
    parser.add_argument("--list", action="store_true",
                        help="List available serial ports and exit")
    parser.add_argument("--get-mac", action="store_true",
                        help="Only read MAC address and exit")
    parser.add_argument("--port",
                        help="Serial port (e.g., COM3, /dev/ttyUSB0)")
    
    args = parser.parse_args()
    
    # List ports and exit
    if args.list:
        list_available_ports()
        sys.exit(0)
    
    # Find ESP port
    port = find_esp_port(args.port)
    if not port:
        print("[PORT] No device found matching ESP USB IDs")
        sys.exit(1)
    
    print(f"[PORT] Serial device found: {port}")

    # Try to read MAC first
    mac = read_mac(port)
    
    # Get MAC only mode
    if mac and args.get_mac:
        sys.exit(0)

    elif args.get_mac:
        sys.exit(1)
    
    # Flash production firmware
    if not mac:
        print("[FLASH] Unknown device, Cannot flash firmware!!")
        sys.exit(1)

    print("[FLASH] Flashing production firmware")
    if not flash_firmware(port, "production"):
        sys.exit(1)
    
    sys.exit(0)

if __name__ == "__main__":
    main()
