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

// === Timer-based trigger handling ===
/**
 * @brief Handles relay trigger with two modes controlled by a flag.
 * @note This function is stateless and determines the desired state
 * based on the current time. Only trigger if its inside the grace period.
 * 
 * @param on_boot If false, the function performs a robust, stateless
 * check to correct the relay's state.
 */
void io_pin_trigger(
    uint8_t enable, uint8_t current_state,
    uint8_t on_hr, uint8_t on_min,
    uint8_t off_hr, uint8_t off_min,
    uint16_t current_hr, uint16_t current_min,
    uint16_t grace_min, bool on_boot,
    uint16_t address, const char *relay_str
) {
    if (!enable) {
        return; // Automation is disabled
    }

    // Convert all times to total minutes for easier comparison
    const uint16_t MINUTES_IN_DAY = 24 * 60;
    uint16_t on_total_mins = on_hr * 60 + on_min;
    uint16_t off_total_mins = off_hr * 60 + off_min;
    uint16_t current_total_mins = current_hr * 60 + current_min;

    if (!on_boot) {
        // Normal operation mode - Check within grace period
        uint16_t minutes_since_on = 
            (current_total_mins - on_total_mins + MINUTES_IN_DAY) % MINUTES_IN_DAY;
        uint16_t minutes_since_off = 
            (current_total_mins - off_total_mins + MINUTES_IN_DAY) % MINUTES_IN_DAY;

        // Check ON trigger within grace period
        if (!current_state && (minutes_since_on < grace_min)) {
            debug_printf("[SYNC] Auto %s ON triggered\n", relay_str);
            vp_set_value(address, 1);
            vp_save_values();
            hmi_update_value(address);
            return;
        }

        // Check OFF trigger within grace period
        if (current_state && (minutes_since_off < grace_min)) {
            debug_printf("[SYNC] Auto %s OFF triggered\n", relay_str);
            vp_set_value(address, 0);
            vp_save_values();
            hmi_update_value(address);
            return;
        }

    } else {
        // Boot-up mode - Correct state based on schedule
        bool desired_state = false;

        if (on_total_mins < off_total_mins) {
            // Same-day schedule (e.g., ON 09:00, OFF 18:00)
            desired_state = ((current_total_mins >= on_total_mins) &&
                             (current_total_mins < off_total_mins));

        } else {
            // Overnight schedule (e.g., ON 21:00, OFF 06:00)
            desired_state = ((current_total_mins >= on_total_mins) ||
                             (current_total_mins < off_total_mins));
        }

        // Update state if it doesn't match the schedule
        if (current_state != desired_state) {
            debug_printf("[SYNC] Auto %s %s triggered (boot)\n", 
                        relay_str, desired_state ? "ON" : "OFF");
            vp_set_value(address, desired_state);
            vp_save_values();
            hmi_update_value(address);
        }
    }
}

// === NTP Client Initialization ===
void ntp_client_init(void) {
    timeClient.begin();
    timeClient.setTimeOffset(NTP_OFFSET);
    timeClient.setUpdateInterval(NTP_UPDATE_INTERVAL);
}

// === NTP Client Update ===
void ntp_client_update(bool force) {
    static uint8_t retry_count = 0;
    const uint8_t MAX_RETRIES = 3;
    
    if(force || !timeClient.isTimeSet() || retry_count < MAX_RETRIES) {
        debug_println("[NTP] Time not set, Calling forceUpdate");
        timeClient.forceUpdate();
        
        // Wait a bit for the update to complete
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if(timeClient.isTimeSet()) {
            debug_printf(
                "[NTP] Time successfully set: %s\n",
                timeClient.getFormattedTime().c_str()
            );
            retry_count = 0; // Reset retry counter on success

        } else {
            retry_count++;
            debug_printf(
                "[NTP] WARNING: Time update failed (attempt %d/%d)\n",
                retry_count,
                MAX_RETRIES
            );
            
            if(retry_count >= MAX_RETRIES) {
                debug_println("[NTP] Maximum retries reached, giving up!!");
            }
        }
    } else {
        timeClient.update();
        debug_printf(
            "[NTP] Current time: %s\n",
            timeClient.getFormattedTime().c_str()
        );
    }
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
