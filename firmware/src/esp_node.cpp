#include "global.h"

extern DWIN hmi;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER);

// === Device Configuration ===
void io_init() {
    pinMode(LIGHT_RELAY, OUTPUT);
    pinMode(WATER_RELAY, OUTPUT);
    pinMode(FAN_RELAY, OUTPUT);

    // Reserve pin for future use
    pinMode(RELAY_PIN_4, OUTPUT);
    digitalWrite(RELAY_PIN_4, LOW);
}

// === Address to Pin Mapping for Relays ===
uint8_t io_pin_map(uint16_t address) {
    switch (address) {
        case VP_LIGHT_STATE:
            return LIGHT_RELAY;

        case VP_WATER_STATE:
            return WATER_RELAY;

        case VP_FAN_STATE:
            return FAN_RELAY;

        default:
            return 0; // No pin mapping
    }
}

// === NTP Client Initialization ===
void ntp_client_init(void) {
    timeClient.begin();
    timeClient.setTimeOffset(NTP_OFFSET);
    timeClient.setUpdateInterval(NTP_UPDATE_INTERVAL);
}

// === NTP Client Update ===
void ntp_client_update(void) {
    if(!timeClient.isTimeSet()) {
        debug_println("[NTP] Time not set, Calling forceUpdate");
        timeClient.forceUpdate();
    } else {
        timeClient.update();
    }
    debug_printf("[NTP] Current time: %s\n", timeClient.getFormattedTime().c_str());
}

// === HMI Initialization ===
void hmi_init() {
    debug_println("[BOOT] Initializing DWIN HMI");
    hmi_update_all();
}

// == Callback function for DWIN events ===
void hmi_on_event(String address, int data, String message, String response) {
    if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
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

        // Update HMI display based on the address
        if (updated) {
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
                    hmi_update_string(VP_GROWTH_STR);

                    vp_save_values();
                    break;
                }

                case VP_LIGHT_STATE:
                    hmi_update_value(VP_LIGHT_STATE);
                    break;

                case VP_WATER_STATE:
                    hmi_update_value(VP_WATER_STATE);
                    break;

                case VP_FAN_STATE:
                    hmi_update_value(VP_FAN_STATE);
                    break;

                default:
                    break;
            }
        }
        xSemaphoreGive(xVPMutex);
    }
}
