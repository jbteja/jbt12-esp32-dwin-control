"""
Test Script for DWIN HMI Serial Communication

About:
- This script builds frames with the DWIN header (0x5A, 0xA5) and a single-byte
    length field plus payload. It does not compute or append any checksum/CRC; if
    your HMI requires one, you must add it.

Usage:
- Edit the `PORT` and `BAUD_RATE` values in `main()` to match your setup.
- Run: `python test_serial.py`
- The script prints the sent frames (hex) and any responses read from the port.

Notes:
- Modify the addresses and data in `main()` for your tests. The script sends one
    read command and one write command by default.
- Response parsing is minimal — read bytes are printed as hexadecimal for
    debugging; you can adapt the parsing for your payload formats.
"""

__author__ = "Bhanu Teja J"
__version__ = "0.0.1"
__created__ = "2025-07-03"
__updated__ = "2025-12-12"

import serial
import time
import binascii

HEADER = b'\x5A\xA5'
CMD_READ = 0x83
CMD_WRITE = 0x82

def send_read_command(ser, address, word_count):
    """Send a read command to the display"""
    
    # Build payload
    payload = bytes([
        CMD_READ,
        (address >> 8) & 0xFF,  # Address high byte
        address & 0xFF,          # Address low byte
        word_count               # Number of words to read
    ])
    
    # Build frame (without checksum)
    length = len(payload)
    frame = HEADER + bytes([length]) + payload
    
    # Send frame
    ser.write(frame)
    print(f"📤 [READ SENT] Addr: 0x{address:04X}, Words: {word_count}")
    print(f"    Hex: {binascii.hexlify(frame).decode('ascii')}")

def send_write_command(ser, address, data):
    """Send a write command to the display"""

    # Build payload
    payload = bytes([
        CMD_WRITE,
        (address >> 8) & 0xFF,  # Address high byte
        address & 0xFF           # Address low byte
    ]) + data
    
    # Build frame (without checksum)
    length = len(payload)
    frame = HEADER + bytes([length]) + payload
    
    # Send frame
    ser.write(frame)
    data_str = ''.join(chr(b) if 32 <= b <= 126 else f'\\x{b:02x}' for b in data)
    print(f"📤 [WRITE SENT] Addr: 0x{address:04X}, Data: {data_str}")
    print(f"    Hex: {binascii.hexlify(frame).decode('ascii')}")

def main():
    """Main function to test serial communication"""
    PORT = 'COM7'  # Change to your serial port
    BAUD_RATE = 115200 # Configure according to your setup
    
    try:
        # Open serial connection
        ser = serial.Serial(PORT, BAUD_RATE, timeout=0.5)
        print(f"Connected to {PORT} at {BAUD_RATE} baud")
        
        # 1. Send read request for TIME (0x1000) - 3 words (6 bytes)
        send_read_command(ser, 0x1000, 3)
        
        # Wait and read response
        time.sleep(0.1)
        response = ser.read(20)  # Read more than expected response size
        if response:
            print(f"📥 [RESPONSE] Hex: {binascii.hexlify(response).decode('ascii')}")

        else:
            print("⏳ No response received for read command")
        
        # 2. Send write command to set TIME
        new_time = "21:43"
        send_write_command(ser, 0x1000, new_time.encode('ascii'))
        
        # Wait and read response
        time.sleep(0.1)
        response = ser.read(20)
        if response:
            print(f"📥 [RESPONSE] Hex: {binascii.hexlify(response).decode('ascii')}")

        else:
            print("⏳ No response received for write command")
        
    except serial.SerialException as e:
        print(f"Serial error: {e}")

    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Port closed")

if __name__ == "__main__":
    main()
