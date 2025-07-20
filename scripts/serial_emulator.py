from collections import OrderedDict


# VP Address Configuration
VP_CONFIG = OrderedDict({
    0x1000: {'name': 'TIME', 'type': 'str', 'length': 5, 'default': "12:34"},
    0x1010: {'name': 'HOSTNAME', 'type': 'str', 'length': 6, 'default': "NGS001"},
    0x1020: {'name': 'PLANT_ID', 'type': 'uint8', 'length': 2, 'default': 0},
    0x1030: {'name': 'TOTAL_CYCLE', 'type': 'uint8', 'length': 2, 'default': 15},
    0x1040: {'name': 'GROWTH_DAY', 'type': 'uint8', 'length': 2, 'default': 7},
    0x1050: {'name': 'GROWTH_BAR', 'type': 'uint8', 'length': 2, 'default': 5},
    0x1060: {'name': 'FW_VERSION', 'type': 'str', 'length': 6, 'default': "v1.0.0"},
    0x1070: {'name': 'HW_VERSION', 'type': 'str', 'length': 6, 'default': "v1.0.0"},

    0x1100: {'name': 'LIGHT_STATE', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1110: {'name': 'LIGHT_AUTO', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1120: {'name': 'LIGHT_ON_HR', 'type': 'uint8', 'length': 2, 'default': 8},
    0x1130: {'name': 'LIGHT_ON_MIN', 'type': 'uint8', 'length': 2, 'default': 0},
    0x1140: {'name': 'LIGHT_OFF_HR', 'type': 'uint8', 'length': 2, 'default': 20},
    0x1150: {'name': 'LIGHT_OFF_MIN', 'type': 'uint8', 'length': 2, 'default': 0},
    
    0x1200: {'name': 'WATER_STATE', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1210: {'name': 'WATER_AUTO', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1220: {'name': 'WATER_ON_HR', 'type': 'uint8', 'length': 2, 'default': 8},
    0x1230: {'name': 'WATER_ON_MIN', 'type': 'uint8', 'length': 2, 'default': 0}, 
    0x1240: {'name': 'WATER_OFF_HR', 'type': 'uint8', 'length': 2, 'default': 20},
    0x1250: {'name': 'WATER_OFF_MIN', 'type': 'uint8', 'length': 2, 'default': 0},
    0x1260: {'name': 'WATER_INTERVAL_HR', 'type': 'uint8', 'length': 2, 'default': 4},
    0x1270: {'name': 'WATER_DURATION_SEC', 'type': 'uint8', 'length': 2, 'default': 10},
    
    0x1300: {'name': 'FAN_STATE', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1310: {'name': 'FAN_AUTO', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1320: {'name': 'FAN_ON_HR', 'type': 'uint8', 'length': 1, 'default': 9},
    0x1330: {'name': 'FAN_ON_MIN', 'type': 'uint8', 'length': 1, 'default': 0},
    0x1340: {'name': 'FAN_OFF_HR', 'type': 'uint8', 'length': 1, 'default': 18},
    0x1350: {'name': 'FAN_OFF_MIN', 'type': 'uint8', 'length': 1, 'default': 0},
    
    0x1400: {'name': 'WIFI_STATE', 'type': 'str', 'length': 16, 'default': "Connected"},
    0x1410: {'name': 'WIFI_SSID', 'type': 'str', 'length': 32, 'default': "MyWiFi_SSID"},
    0x1420: {'name': 'WIFI_PASSWORD', 'type': 'str', 'length': 32, 'default': ""},
    0x1430: {'name': 'IP_ADDRESS', 'type': 'str', 'length': 16, 'default': "- - - -"},
    0x1440: {'name': 'SIGNAL_STRENGTH', 'type': 'str', 'length': 16, 'default': "Strong"},
    0x1450: {'name': 'CONFIG_NETWORK', 'type': 'uint8', 'length': 1, 'default': 0},
})

# VP Address Class for macro-like functionality
class VPAddresses:
    def __init__(self, config):
        self._config = config
        self._values = {}
        # Initialize with default values
        for addr, info in config.items():
            self._values[addr] = info['default']
    
    def __getattr__(self, name):
        # Find the address for the given VP name
        for addr, info in self._config.items():
            if info['name'] == name:
                return self._values[addr]
        raise AttributeError(f"VP address '{name}' not found")
    
    def __setattr__(self, name, value):
        if name.startswith('_'):
            super().__setattr__(name, value)
            return
            
        # Find the address for the given VP name
        for addr, info in self._config.items():
            if info['name'] == name:
                self._values[addr] = value
                return
        raise AttributeError(f"VP address '{name}' not found")
    
    def get_address(self, name):
        # Get the address for a VP name
        for addr, info in self._config.items():
            if info['name'] == name:
                return addr
        return None

# Create a global instance
VP = VPAddresses(VP_CONFIG)

# Example usage:
# if VP.TIME:
#     print(f"Current time is {VP.TIME}")
# 
# VP.LIGHT_STATE = 1  # Turn on light
