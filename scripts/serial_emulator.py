"""
Serial monitor and emulator

About:
- Emulates and monitors basic DWIN HMI serial traffic. Parses incoming
    frames, updates a virtual VP (variable pointer) registry, and can send
    read/write frames for manual testing.

Usage:
- Run the monitor: `python scripts/serial_emulator.py --port COM7 --baud 115200`
- Use `--help` to see command line options.

Notes:
- Frames built by this script do not include an additional checksum by
    default in transmitted frames.
"""

__author__ = "Bhanu Teja J"
__version__ = "0.0.3"
__created__ = "2025-07-11"
__updated__ = "2025-12-12"

import sys
import time
import serial
import binascii
import argparse
from datetime import datetime
from collections import OrderedDict

# ==================================================
# VP Address Configuration
# ==================================================
VP_CONFIG = OrderedDict({
    0x1000: {'name': 'TIME', 'type': 'str', 'length': 6, 'default': "12:34"},
    0x1010: {'name': 'HOSTNAME', 'type': 'str', 'length': 6, 'default': "NGS001"},
    0x1020: {'name': 'PLANT_ID', 'type': 'uint8', 'length': 1, 'default': 3},
    0x1030: {'name': 'TOTAL_CYCLE', 'type': 'uint8', 'length': 1, 'default': 15},
    0x1040: {'name': 'GROWTH_DAY', 'type': 'uint8', 'length': 1, 'default': 6},
    0x1050: {'name': 'GROWTH_BAR', 'type': 'uint8', 'length': 1, 'default': 10},
    0x1060: {'name': 'GROWTH_STR', 'type': 'str', 'length': 6, 'default': "12/15"},
    0x1070: {'name': 'FW_VERSION', 'type': 'str', 'length': 6, 'default': "v1.0.0"},
    0x1080: {'name': 'HW_VERSION', 'type': 'str', 'length': 6, 'default': "v1.0.0"},

    0x1100: {'name': 'LIGHT_STATE', 'type': 'uint8', 'length': 1, 'default': 1},
    0x1110: {'name': 'LIGHT_AUTO', 'type': 'uint8', 'length': 1, 'default': 1},
    0x1120: {'name': 'LIGHT_ON_HR', 'type': 'uint8', 'length': 1, 'default': 8},
    0x1130: {'name': 'LIGHT_ON_MIN', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1140: {'name': 'LIGHT_OFF_HR', 'type': 'uint8', 'length': 1, 'default': 20},
    0x1150: {'name': 'LIGHT_OFF_MIN', 'type': 'uint8', 'length': 1, 'default': 0},
    
    0x1200: {'name': 'WATER_STATE', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1210: {'name': 'WATER_AUTO', 'type': 'uint8', 'length': 1, 'default': 1},
    0x1220: {'name': 'WATER_ON_HR', 'type': 'uint8', 'length': 1, 'default': 8},
    0x1230: {'name': 'WATER_ON_MIN', 'type': 'uint8', 'length': 1, 'default': 0}, 
    0x1240: {'name': 'WATER_OFF_HR', 'type': 'uint8', 'length': 1, 'default': 20},
    0x1250: {'name': 'WATER_OFF_MIN', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1260: {'name': 'WATER_INTERVAL_HR', 'type': 'uint8', 'length': 1, 'default': 4},
    0x1270: {'name': 'WATER_DURATION_SEC', 'type': 'uint8', 'length': 1, 'default': 10},
    
    0x1300: {'name': 'FAN_STATE', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1310: {'name': 'FAN_AUTO', 'type': 'uint8', 'length': 1, 'default': 1},
    0x1320: {'name': 'FAN_ON_HR', 'type': 'uint8', 'length': 1, 'default': 9},
    0x1330: {'name': 'FAN_ON_MIN', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1340: {'name': 'FAN_OFF_HR', 'type': 'uint8', 'length': 1, 'default': 18},
    0x1350: {'name': 'FAN_OFF_MIN', 'type': 'uint8', 'length': 1, 'default': 0},
    
    0x1400: {'name': 'WIFI_STATE', 'type': 'str', 'length': 16, 'default': "Connected"},
    0x1410: {'name': 'WIFI_SSID', 'type': 'str', 'length': 32, 'default': "NGS"},
    0x1420: {'name': 'WIFI_PASSWORD', 'type': 'str', 'length': 32, 'default': ""},
    0x1430: {'name': 'IP_ADDRESS', 'type': 'str', 'length': 16, 'default': "- - - -"},
    0x1440: {'name': 'SIGNAL_STRENGTH', 'type': 'str', 'length': 16, 'default': "Strong"},
    0x1450: {'name': 'CONFIG_NETWORK', 'type': 'uint8', 'length': 1, 'default': 0},
})

# ==================================================
# VP Address Management Class
# ==================================================
class VPAddresses:
    """Class to manage VP addresses and their values"""
    def __init__(self, config):
        self._config = config
        self._values = {}
        self._name_to_addr = {}
        self._addr_info = {}
        
        # Create mappings and initialize values
        for addr, info in config.items():
            self._values[addr] = info['default']
            self._addr_info[addr] = info
            
            name = info['name']
            if name in self._name_to_addr:
                raise ValueError(f"Duplicate VP name: {name}")
            self._name_to_addr[name] = addr
    
    def __getattr__(self, name):
        if name in self._name_to_addr:
            addr = self._name_to_addr[name]
            return self._values[addr]
        raise AttributeError(f"VP address '{name}' not found")
    
    def __setattr__(self, name, value):
        if name.startswith('_'):
            super().__setattr__(name, value)
            return
            
        if name in self._name_to_addr:
            addr = self._name_to_addr[name]
            info = self._addr_info[addr]
            
            # Validate and convert value
            if info['type'] == 'str':
                if not isinstance(value, str):
                    raise TypeError(f"{name} requires string value")
                # Truncate to max length
                value = value[:info['length']]
            elif info['type'].startswith('uint'):
                if not isinstance(value, int):
                    raise TypeError(f"{name} requires integer value")
                # Get bit size and validate range
                bits = int(info['type'][4:])
                max_val = (1 << bits) - 1
                if value < 0 or value > max_val:
                    raise ValueError(f"{name} value out of range (0-{max_val})")
            
            self._values[addr] = value
        else:
            raise AttributeError(f"VP address '{name}' not found")
    
    def get_address(self, name):
        """Get VP address by name"""
        return self._name_to_addr.get(name)
    
    def get_name_by_address(self, address):
        """Get VP name by address"""
        info = self._addr_info.get(address)
        return info['name'] if info else None
    
    def get_type_by_address(self, address):
        """Get VP type by address"""
        info = self._addr_info.get(address)
        return info['type'] if info else None

# Create a global instance
VP = VPAddresses(VP_CONFIG)

# Protocol Constants
HEADER = b'\x5A\xA5'
CMD_WRITE = 0x82 # Writes data to DWIN
CMD_READ = 0x83 # Reads data from DWIN

# ==================================================
# DWIN Handler Class
# ==================================================
class DWINHandler:
    """Handler for DWIN serial communication"""
    def __init__(self, serial_port):
        self.ser = serial_port
        self.callbacks = {}
        self.last_frame = None
        
    def register_callback(self, vp_address, callback):
        """Register a callback for a specific VP address"""
        if vp_address not in self.callbacks:
            self.callbacks[vp_address] = []
        self.callbacks[vp_address].append(callback)
    
    def handle_frame(self, frame):
        """
        Process frame and trigger callbacks
        Returns decoded (command, address, data) tuple
        """
        self.last_frame = frame
        if len(frame) < 6:
            return None
        
        # Parse frame components
        command = frame[3]
        address = (frame[4] << 8) | frame[5]
        data_bytes = bytes(frame[6:])  # Exclude checksum
        
        # Get VP info
        vp_name = VP.get_name_by_address(address)
        vp_type = VP.get_type_by_address(address) if address in VP_CONFIG else None
        
        # Convert data to appropriate type
        value = None
        if vp_type == 'str' and address in VP_CONFIG:
            # Handle strings with proper length and null termination
            max_len = VP_CONFIG[address]['length']
            # Pad to full length and decode
            padded = data_bytes.ljust(max_len, b'\x00')[:max_len]
            try:
                value = padded.decode('ascii', errors='ignore').rstrip('\x00')
            except UnicodeDecodeError:
                value = str(data_bytes)  # Fallback to raw representation
        elif vp_type == 'uint8':
            if data_bytes:
                value = data_bytes[0]
        elif vp_type == 'uint16':
            if len(data_bytes) >= 2:
                value = int.from_bytes(data_bytes[:2], 'big')
        
        # Update VP storage if valid value
        if vp_name and value is not None:
            try:
                setattr(VP, vp_name, value)
                print(f"📥 [VP UPDATE] {vp_name} = {value}")
            except (TypeError, ValueError) as e:
                print(f"⚠️ [VP UPDATE ERROR] {vp_name}: {e}")
        
        # Trigger callbacks for this address
        if address in self.callbacks:
            for callback in self.callbacks[address]:
                callback(command, address, data_bytes, value)
                
        return (command, address, data_bytes, value)
    
    def send_write_command(self, vp_name, value):
        """Send a write command to the display"""
        address = VP.get_address(vp_name)
        if address is None:
            print(f"⚠️ [SEND ERROR] Unknown VP: {vp_name}")
            return
            
        # Convert value to bytes based on type
        vp_type = VP.get_type_by_address(address)
        if vp_type == 'str':
            data = value.encode('ascii').ljust(VP_CONFIG[address]['length'], b'\x00')
        elif vp_type == 'uint8':
            data = bytes([value])
        elif vp_type == 'uint16':
            data = value.to_bytes(2, 'big')
        else:
            print(f"⚠️ [SEND ERROR] Unsupported type for {vp_name}")
            return
        
        # Build frame without checksum
        payload = bytes([CMD_WRITE, (address >> 8) & 0xFF, address & 0xFF]) + data
        length = len(payload)
        frame = HEADER + bytes([length]) + payload
        
        # Send frame
        self.ser.write(frame)
        print(f"📤 [SENT] {vp_name} = {value}")
        print(f"    Hex: {binascii.hexlify(frame).decode('ascii')}")

    def send_read_command(self, vp_name=None, address=None, word_count=1):
        """
        Send a read command to the display
        """
        # Resolve address from VP name
        if vp_name is not None:
            address = VP.get_address(vp_name)
            if address is None:
                print(f"⚠️ [READ ERROR] Unknown VP: {vp_name}")
                return
            vp_info = VP_CONFIG.get(address)
            if vp_info:
                if vp_info['type'] == 'str':
                    word_count = (vp_info['length'] + 1) // 2
                else:
                    word_count = vp_info['length']
        
        if address is None:
            print("⚠️ [READ ERROR] No address specified")
            return
        
        if word_count < 1 or word_count > 255:
            print(f"⚠️ [READ ERROR] Invalid word count: {word_count}")
            return
        
        # Build frame without checksum (matches working example)
        payload = bytes([
            CMD_READ,
            (address >> 8) & 0xFF,
            address & 0xFF,
            word_count
        ])
        
        length = len(payload)
        frame = HEADER + bytes([length]) + payload
        
        # Send frame
        self.ser.write(frame)
        print(f"📤 [READ REQUEST] Addr: 0x{address:04X}, Words: {word_count}")
        print(f"    Hex: {binascii.hexlify(frame).decode('ascii')}")

    def decode_frame(self, frame):
        """Decode and print frame structure"""
        if len(frame) < 5:
            print("  [Frame too short]")
            return
        
        header = frame[:2]
        if header != [HEADER[0], HEADER[1]]:  # [0x5A, 0xA5]
            print("  [Invalid header]")
            return
        
        length = frame[2]
        command = frame[3]
        address = (frame[4] << 8) | frame[5]  # Combine high and low bytes
        
        # Adjust expected length (no checksum)
        expected_length = 3 + length
        if len(frame) != expected_length:
            print(f"  [Invalid length: expected {expected_length}, got {len(frame)}]")
            return
        
        # Verify checksum
        calc_checksum = sum(frame[2:-1]) & 0xFF
        received_checksum = frame[-1]
        
        # Print structured information
        print(f"CMD: 0x{command:02X}")
        
        # Identify command type
        cmd_type = "Unknown"
        if command == CMD_READ:
            cmd_type = "Read"
        elif command == CMD_WRITE:
            cmd_type = "Write"
        print(f"Command Type: {cmd_type}")
        
        # Check if the VP address is known in our configuration
        vp_name = VP.get_name_by_address(address)
        if vp_name:
            print(f"ADDR: 0x{address:04X} (Known VP: {vp_name})")
        else:
            print(f"ADDR: 0x{address:04X} (Unknown VP)")
        
        # Print data section if present
        if len(frame) > 6:
            data = frame[6:-1]  # Exclude header and checksum
            print("DATA: ", end='')
            for i, byte in enumerate(data):
                ascii_val = chr(byte) if 32 <= byte <= 126 else '.'
                print(f"0x{byte:02X} ({ascii_val})", end=' ')
            print()
            
            # If this is a known VP, try to interpret the data according to its type
            if vp_name and address in VP_CONFIG:
                vp_info = VP_CONFIG[address]
                vp_type = vp_info['type']
                
                print("INTERPRETED: ", end='')
                if vp_type == 'str':
                    # Convert bytes to string with proper handling
                    max_len = vp_info['length']
                    padded = bytes(data).ljust(max_len, b'\x00')[:max_len]
                    try:
                        str_value = padded.decode('ascii', errors='ignore').rstrip('\x00')
                        print(f"String: '{str_value}'")
                    except:
                        print("Unable to decode as string")
                elif vp_type == 'uint8':
                    if data:
                        print(f"Value: {data[0]}")
                    else:
                        print("No data")
                elif vp_type == 'uint16':
                    if len(data) >= 2:
                        value = (data[0] << 8) | data[1]
                        print(f"Value: {value}")
                    else:
                        print("Insufficient data for uint16")
                else:
                    print(f"Type '{vp_type}' interpretation not implemented")
        
        # Print checksum status
        if calc_checksum != received_checksum:
            print(f"CHECKSUM: ERROR (expected 0x{calc_checksum:02X}, got 0x{received_checksum:02X})")
        else:
            print("CHECKSUM: OK")
        
        print()  # Add blank line between frames

# ==================================================
# Frame Detector Class
# ==================================================
class FrameDetector:
    """Detects and extracts frames from incoming byte stream"""
    def __init__(self):
        self.state = "IDLE"
        self.buffer = []
        self.length = 0
        self.bytes_needed = 0
        self.last_byte_time = time.time()
    
    def process_byte(self, byte):
        """Process incoming byte and detect frames"""
        self.last_byte_time = time.time()
        
        if self.state == "IDLE":
            if byte == HEADER[0]:  # 0x5A
                self.state = "GOT_5A"
                self.buffer = [byte]
        elif self.state == "GOT_5A":
            if byte == HEADER[1]:  # 0xA5
                self.state = "IN_FRAME"
                self.buffer.append(byte)
            else:
                self.reset()
        elif self.state == "IN_FRAME":
            self.buffer.append(byte)
            
            # Get length after we have 3 bytes (5A, A5, len)
            if len(self.buffer) == 3:
                self.length = byte
                self.bytes_needed = self.length # No checksum
            
            # Check if frame is complete
            if len(self.buffer) >= 4 and len(self.buffer) == (3 + self.bytes_needed):
                frame = self.buffer.copy()
                self.reset()
                return frame
        
        return None
    
    def reset(self):
        """Reset the frame detector state"""
        self.state = "IDLE"
        self.buffer = []
        self.length = 0
        self.bytes_needed = 0
    
    def check_timeout(self, timeout=0.5):
        """Check if frame detection has timed out"""
        if self.state != "IDLE" and (time.time() - self.last_byte_time) > timeout:
            buffer_copy = self.buffer.copy()
            self.reset()
            return buffer_copy
        return None

def process_serial_stream(port_name, baud_rate):
    """
    Monitors and processes DWIN display communication over serial.
    
    This function:
    1. Checks communication with the display
    2. Initializes display values
    3. Enters the main monitoring loop
    """
    try:
        # Create serial connection
        ser = serial.Serial(port_name, baud_rate, timeout=0.1)
        print(f"Connected to {port_name} at {baud_rate} baud...")
        
        # Create handler instance
        handler = DWINHandler(ser)
        
        # ==================================================
        # 1. Communication Check
        # ==================================================
        print("Checking display communication...")
        communication_ok = False
        detector = FrameDetector()
        
        for attempt in range(3):
            print(f"Attempt {attempt+1}: Sending read request for TIME (0x1000)")
            handler.send_read_command('TIME')
            
            start_time = time.time()
            while time.time() - start_time < 1.0:
                data = ser.read(1)
                if data:
                    frame = detector.process_byte(data[0])
                    if frame:
                        if len(frame) >= 6 and frame[3] == CMD_READ and (frame[4] << 8 | frame[5]) == 0x1000:
                            print("✅ Communication established with display")
                            communication_ok = True
                            break
                
                timeout_frame = detector.check_timeout()
                if timeout_frame:
                    print("⚠️ Frame detection timeout, resetting state")
            
            if communication_ok:
                break
        
        if not communication_ok:
            print("❌ Failed to establish communication with display")
            ser.close()
            return
        
        # ==================================================
        # 2. Initialization Phase
        # ==================================================
        print("\nInitializing display values...")
        
        # Register callback for LIGHT_AUTO
        def light_auto_callback(cmd, address, data, value):
            if cmd == CMD_WRITE:
                print(f"🎯 [CUSTOM HANDLER] Light Auto changed to {value}")
                # Add custom actions here
                # Example: Turn on fan when light auto mode is enabled
                if value == 1:
                    handler.send_write_command('FAN_AUTO', 1)
        
        handler.register_callback(0x1110, light_auto_callback)
        
        # List of VPs to initialize
        init_vps = [
            'HOSTNAME', 'PLANT_ID', 'TOTAL_CYCLE', 'GROWTH_DAY',
            'GROWTH_BAR', 'FW_VERSION', 'HW_VERSION'
        ]
        
        # Send initial values
        for vp_name in init_vps:
            try:
                value = getattr(VP, vp_name)
                handler.send_write_command(vp_name, value)
                print(f"  - {vp_name}: {value}")
                time.sleep(0.1)  # Small delay between commands
            except Exception as e:
                print(f"⚠️ Failed to initialize {vp_name}: {e}")
        
        # Send initial time
        current_time_str = datetime.now().strftime("%H:%M")
        handler.send_write_command('TIME', current_time_str)
        print(f"  - TIME: {current_time_str}")
        print("Initialization complete\n")
        
        # ==================================================
        # 3. Main Monitoring Loop
        # ==================================================
        print("Starting main monitoring loop. Press Ctrl+C to exit...")
        detector = FrameDetector()
        last_time_update = time.time()
        update_interval = 15  # Seconds
        
        while True:
            data = ser.read(1)
            current_time = time.time()

            # Update time on display periodically
            if current_time - last_time_update >= update_interval:
                current_time_str = datetime.now().strftime("%H:%M")
                handler.send_write_command('TIME', current_time_str)
                last_time_update = current_time
            
            if data:
                byte = data[0]
                frame = detector.process_byte(byte)
                
                if frame:
                    # Print RAW frame
                    print("RAW: ", end='')
                    for i, b in enumerate(frame):
                        print(f"0x{b:02X}", end=' ')
                        if (i + 1) % 8 == 0 and i != len(frame) - 1:
                            print("\n     ", end='')
                    print()
                    
                    # Handle the frame
                    handler.handle_frame(frame)
            
            # Check for frame timeout
            timeout_frame = detector.check_timeout()
            if timeout_frame:
                print("RAW: ", end='')
                for b in timeout_frame:
                    print(f"0x{b:02X}", end=' ')
                print(" <-- Incomplete frame (timeout)")
            
            time.sleep(0.001)
            
    except serial.SerialException as e:
        print(f"\nSerial error: {e}")
    except KeyboardInterrupt:
        print("\n\nMonitoring stopped")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Port closed")

# ==================================================
# Main Entry Point
# ==================================================
def main():
    parser = argparse.ArgumentParser(
        description="DWIN Monitor - Monitor VP updates from DWIN display"
    )
    parser.add_argument(
        "--port", "-p", required=True, help="Serial COM port (Windows: COMx, Linux: /dev/ttyUSB0, Mac: /dev/cu.usbserial-*)"
    )
    parser.add_argument(
        "--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)"
    )

    # Show help if no args were passed
    if len(sys.argv) == 1:
        parser.print_help(sys.stderr)
        sys.exit(1)

    args = parser.parse_args()

    # Start monitoring
    process_serial_stream(args.port, args.baud)

if __name__ == "__main__":
    main()
