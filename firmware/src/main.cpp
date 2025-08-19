#include "common.h"

// === DWIN HMI Initialization ===
DWIN hmi(DGUS_SERIAL, 16, 17, DGUS_BAUD); // Serial2 16 as Rx and 17 as Tx

// == Callback function for DWIN events ===
void onHMIEvent(String address, int data, String message, String response) {
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
            hmi_update_value(VP_GROWTH_BAR, 0);

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

void setup() {
    debug_begin(115200);
    debug_println("Initializing communication...");
    delay(1000);

    // Initialize DWIN HMI
    hmi.echoEnabled(false);
    hmi.hmiCallBack(onHMIEvent);
    delay(500);

    // Load VP values from NVS and save defaults
    vp_load_values();
    if (strlen(vp.hw_version) == 0 || strcmp(vp.hw_version, HW_VERSION) != 0) {
        vp_set_string(VP_HW_VERSION, HW_VERSION);
    }

    if (strlen(vp.fw_version) == 0 || strcmp(vp.fw_version, FW_VERSION) != 0) {
        vp_set_string(VP_FW_VERSION, FW_VERSION);
    }

    if (vp.time_str[0] == '\0') {
        snprintf(vp.time_str, sizeof(vp.time_str), "00:00");
    }

    // Save and print initial values
    vp_save_values();
    debug_printf("Hostname: %s\n", vp.hostname);
    debug_printf("HW Version: %s\n", vp.hw_version);
    debug_printf("FW Version: %s\n", vp.fw_version);
    debug_printf(
        "Light: %d, Water: %d, Fan: %d\n",
        vp.light_state,
        vp.water_state,
        vp.fan_state
    );

    // Initialize IO pins and HMI
    io_init();
    hmi_init();

    debug_println("System ready. Starting main loop...");
    //hmi.beepHMI();
}

void loop() {
    hmi.listen();
}
