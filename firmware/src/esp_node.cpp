#include "global.h"

extern DWIN hmi;

// === Device Configuration ===
void io_init() {
    pinMode(LIGHT_RELAY, OUTPUT);
    pinMode(WATER_RELAY, OUTPUT);
    pinMode(FAN_RELAY, OUTPUT);

    // Reserve pin for future use
    pinMode(RELAY_PIN_4, OUTPUT);
    digitalWrite(RELAY_PIN_4, LOW);
}

// === Update HMI Text Fields ===
void hmi_update_text(uint16_t address) {
    const char* str = vp_get_string(address);
    size_t maxlen = 0;

    // Find the storage_size for this address
    for (size_t i = 0; i < num_vp_items; i++) {
        if (vp_items[i].address == address && vp_items[i].type == VP_STRING) {
            maxlen = vp_items[i].storage_size;
            break;
        }
    }

    if (str && maxlen > 0) {
        size_t real_len = strnlen(str, maxlen);

        // Create padded string with spaces
        String padded_str;
        padded_str.reserve(maxlen);
        padded_str = String(str).substring(0, real_len);  // Ensure not exceeding real_len
        while (padded_str.length() < maxlen) {
            padded_str += ' ';
        }

        hmi.setText(address, padded_str);
    } else {
        // Send blank string with full padding
        String blank(maxlen, ' ');
        hmi.setText(address, blank);
    }

    delay(30); // Allow time for HMI to process
}

// === Set Relay and Update HMI Display ===
void hmi_update_value(uint16_t address, uint8_t pin) {
    uint8_t val = vp_get_value(address);
    hmi.setVP(address, val);
    if (pin != 0) {
        digitalWrite(pin, val);
    }
    delay(30);
}

// === HMI Initialization ===
void hmi_init() {
    debug_println("Initializing DWIN HMI...");
    hmi_update_text(VP_TIME);
    hmi_update_text(VP_HOSTNAME);
    hmi_update_value(VP_PLANT_ID);
    hmi_update_value(VP_TOTAL_CYCLE);
    hmi_update_value(VP_GROWTH_DAY);
    hmi_update_value(VP_GROWTH_BAR);
    hmi_update_text(VP_GROWTH_STR);
    hmi_update_text(VP_FW_VERSION);
    hmi_update_text(VP_HW_VERSION);

    hmi_update_value(VP_LIGHT_STATE, LIGHT_RELAY);
    hmi_update_value(VP_LIGHT_AUTO);
    hmi_update_value(VP_LIGHT_ON_HR);
    hmi_update_value(VP_LIGHT_ON_MIN);
    hmi_update_value(VP_LIGHT_OFF_HR);
    hmi_update_value(VP_LIGHT_OFF_MIN);
        
    hmi_update_value(VP_WATER_STATE, WATER_RELAY);
    hmi_update_value(VP_WATER_AUTO);
    hmi_update_value(VP_WATER_ON_HR);
    hmi_update_value(VP_WATER_ON_MIN);
    hmi_update_value(VP_WATER_OFF_HR);
    hmi_update_value(VP_WATER_OFF_MIN);
    hmi_update_value(VP_WATER_INTERVAL_HR);
    hmi_update_value(VP_WATER_DURATION_SEC);

    hmi_update_value(VP_FAN_STATE, FAN_RELAY);
    hmi_update_value(VP_FAN_AUTO);
    hmi_update_value(VP_FAN_ON_HR);
    hmi_update_value(VP_FAN_ON_MIN);
    hmi_update_value(VP_FAN_OFF_HR);
    hmi_update_value(VP_FAN_OFF_MIN);
}

// == Callback function for DWIN events ===
void hmi_on_event(String address, int data, String message, String response) {
    uint16_t vp_addr = strtol(address.c_str(), NULL, 16);
    bool updated = false;
    
    // Lookup item in VP list
    for (size_t i = 0; i < num_vp_items; i++) {
        if (vp_items[i].address == vp_addr) {
            if (vp_items[i].type == VP_UINT8) {
                uint8_t new_val = (uint8_t)data;
                updated = vp_sync_item(vp_addr, &new_val);

            } else if (vp_items[i].type == VP_STRING) {
                updated = vp_sync_item(vp_addr, message.c_str());
            }
            break;
        }
    }

    if (!updated) return;
    
    // Update HMI display based on the address
    switch (vp_addr) {
        case VP_TOTAL_CYCLE:
        case VP_GROWTH_DAY: {
            uint8_t bar = 1; // Calculate growth bar (1-20)
            if (vp.total_cycle > 0) {
                bar = (uint8_t)round((vp.growth_day / (float)vp.total_cycle) * 20.0f);
                if (bar < 1) bar = 1;
                if (bar > 20) bar = 20;
            }
            vp.growth_bar = bar;
            hmi_update_value(VP_GROWTH_BAR);

            snprintf(vp.growth_str, sizeof(vp.growth_str), "%d/%d", vp.growth_day, vp.total_cycle);
            hmi_update_text(VP_GROWTH_STR);

            vp_save_values(); // Save updated values
            break;
        }
        case VP_LIGHT_STATE:
            hmi_update_value(VP_LIGHT_STATE, LIGHT_RELAY);
            break;

        case VP_WATER_STATE:
            hmi_update_value(VP_WATER_STATE, WATER_RELAY);
            break;
        
        case VP_FAN_STATE:
            hmi_update_value(VP_FAN_STATE, FAN_RELAY);
            break;
        
        default:
            break;
    }
}
