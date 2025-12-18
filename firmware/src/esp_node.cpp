#include "global.h"

extern DWIN hmi;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER);

// === Ordinal Suffix Helper ===
const char *ordinal(uint16_t n) {
    if ((n % 100) >= 11 && (n % 100) <= 13) {
        return "th";
    }

    switch (n % 10) {
        case 1:
            return "st";
        case 2:
            return "nd";
        case 3:
            return "rd";
        default:
            return "th";
    }
}

// === Device Configuration ===
void io_init(void) {
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
 * @brief Trigger relay based on current time and schedule and Only
 * updates if the state needs to change.
 * @note This function is stateless and determines the desired state
 * based on the current time. Only trigger if its inside the grace period.
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

    uint8_t desired_state = current_state;
    const uint16_t MINUTES_IN_DAY = 24 * 60;
    uint16_t on_total_mins = on_hr * 60 + on_min;
    uint16_t off_total_mins = off_hr * 60 + off_min;
    uint16_t total_mins = current_hr * 60 + current_min;
    
    if (on_total_mins == off_total_mins) {
        return; // Skip if condition is invalid

    } else if (!on_boot) {
        // Normal operation mode - Check within grace period
        uint16_t minutes_since_on = 
            (total_mins - on_total_mins + MINUTES_IN_DAY) % MINUTES_IN_DAY;
        uint16_t minutes_since_off = 
            (total_mins - off_total_mins + MINUTES_IN_DAY) % MINUTES_IN_DAY;

        // Determine desired state based on grace periods
        if (!current_state && (minutes_since_on <= grace_min)) {
            desired_state = 1; // Should be ON

        } else if (current_state && (minutes_since_off <= grace_min)) {
            desired_state = 0; // Should be OFF
        }

    } else {
        // Boot-up mode - Correct state based on schedule
        if (on_total_mins < off_total_mins) {
            // Same-day schedule (e.g., ON 09:00, OFF 18:00)
            desired_state = ((total_mins >= on_total_mins) &&
                             (total_mins < off_total_mins));

        } else {
            // Overnight schedule (e.g., ON 21:00, OFF 06:00)
            desired_state = ((total_mins >= on_total_mins) ||
                             (total_mins < off_total_mins));
        }
    }

    // Only update if state actually needs to change
    if (current_state != desired_state) {
        debug_printf("[SYNC] Triggered auto %s %s%s\n", 
                    relay_str, desired_state ? "ON" : "OFF",
                    on_boot ? " (boot)" : "");
        
        vp_set_value(address, desired_state);
        vp_save_values();
        hmi_update_value(address);
    }
}

// === Water spray trigger handling ===
/**
 * @brief Handles water spray trigger with interval-based control and
 * Only updates if the state needs to change.
 */
void io_pin_trigger_interval(
    uint8_t enable, uint8_t current_state,
    uint8_t on_hr, uint8_t on_min,
    uint8_t off_hr, uint8_t off_min,
    uint16_t current_hr, uint16_t current_min,
    uint16_t current_sec, uint16_t interval_hr,
    uint16_t duration_sec, uint16_t address, 
    const char *relay_str, uint32_t *last_spray  // Persistent
) {
    if (!enable) {
        return; // Automation is disabled
    }

    if (duration_sec < 1 || duration_sec > 99) {
        debug_printf(
            "[SYNC] Error %s duration must be 1-99 seconds\n", relay_str);
        return;
    }

    if (interval_hr < 1 || interval_hr > 12) {
        debug_printf(
            "[SYNC] Error %s interval must be 1-12 hours\n", relay_str);
        return;
    }
    
    bool in_schedule = false;
    const uint16_t MINUTES_IN_DAY = 24 * 60;
    uint16_t on_total_mins = on_hr * 60 + on_min;
    uint16_t off_total_mins = off_hr * 60 + off_min;
    uint16_t total_mins = current_hr * 60 + current_min;
    
    if (on_total_mins == off_total_mins) {
        // ON==OFF -> treat as disabled schedule
        in_schedule = false;

    } else if (on_total_mins < off_total_mins) {
        // Same-day schedule
        in_schedule = ((total_mins >= on_total_mins) &&
                       (total_mins < off_total_mins));

    } else {
        // Overnight schedule  
        in_schedule = ((total_mins >= on_total_mins) ||
                       (total_mins < off_total_mins));
    }

    uint8_t desired_state = current_state;

    if (!in_schedule) {
        // Outside spray schedule - desired state is OFF
        desired_state = 0;

        // Reset spray timer when outside schedule
        if (*last_spray != 0) {
            *last_spray = 0;
            debug_printf("[SYNC] Auto %s timer reset!\n", relay_str);
        }

    } else {
        // Inside schedule - calculate desired state based on timing
        uint32_t current_time_sec = 
            (current_hr * 3600) + (current_min * 60) + current_sec;
        uint32_t interval_sec = interval_hr * 3600;
        
        if (*last_spray == 0) {
            // First spray - should be ON
            desired_state = 1;
            *last_spray = current_time_sec;

        } else {
            // Calculate time since last spray started
            uint32_t time_since_last_spray;
            
            if (current_time_sec >= *last_spray) {
                time_since_last_spray = current_time_sec - *last_spray;

            } else {
                // Handle midnight rollover
                time_since_last_spray = (86400 - *last_spray) + current_time_sec;
            }

            if (current_state == 1) {
                // Currently spraying - check if duration is complete
                if (time_since_last_spray >= duration_sec) {
                    desired_state = 0; // Should turn OFF
                    *last_spray = current_time_sec; // Reset for next interval
                }

            } else {
                // Not spraying - check if interval has passed
                if (time_since_last_spray >= interval_sec) {
                    desired_state = 1; // Should turn ON
                    *last_spray = current_time_sec; // Reset spray start time
                }
            }
        }
    }

    // Only update if state actually needs to change
    if (current_state != desired_state) {
        debug_printf("[SYNC] Triggered auto %s %s\n", 
                    relay_str, desired_state ? "ON" : "OFF");
        
        vp_set_value(address, desired_state);
        vp_save_values();
        hmi_update_value(address);
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
    const uint8_t MAX_RETRIES = 3;
    const uint32_t RETRY_DELAY_MS = 1000;
    const uint32_t MAX_RETRY_DELAY_MS = 6000;
    
    // If time is already synced and no force update, skip
    if (timeClient.isTimeSet() && !force) {
        timeClient.update();
        // debug_printf("[NTP] Time sync maintained: %s\n", 
        //             timeClient.getFormattedTime().c_str());
        return;
    }
    
    debug_println("[NTP] Time not in sync, calling forceUpdate!");
    
    
    // Blocking call, be careful with delays
    for (uint8_t attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        // Calculate exponential backoff delay
        uint32_t retry_delay_ms = RETRY_DELAY_MS * (1 << (attempt - 1));
        if (retry_delay_ms > MAX_RETRY_DELAY_MS) {
            retry_delay_ms = MAX_RETRY_DELAY_MS;
        }
        
        debug_printf(
            "[NTP] Updating time from server failed (attempt %d/%d)\n",
            attempt,
            MAX_RETRIES
        );
        
        // Try to update
        if (timeClient.forceUpdate()) {
            // Give it time to complete
            vTaskDelay(pdMS_TO_TICKS(500));
            
            if (timeClient.isTimeSet()) {
                debug_printf("[NTP] Time successfully set: %s\n",
                            timeClient.getFormattedTime().c_str());
                return; // Success
            }
        }
        
        // If this wasn't the last attempt, wait before retrying
        if (attempt < MAX_RETRIES) {
            debug_printf("[NTP] Update failed, retrying in %lu ms\n", retry_delay_ms);
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        }
    }
    
    // All retries failed
    debug_println("[NTP] Error: Maximum attempt reached, giving up!!");
}

// === Time Validation Helper ===
bool is_valid_time(int hours, int minutes, int seconds) {
    return (hours >= 0 && hours <= 23 &&
            minutes >= 0 && minutes <= 59 &&
            seconds >= 0 && seconds <= 59);
}

// === Growth & Progress Update ===
void vp_growth_bar_update(void) {
    uint8_t bar = 1; 
    if (vp.total_cycle > 0) {
        // Progress is segmented into 20 parts (1-20)
        bar = (uint8_t)round((vp.growth_day / (float)vp.total_cycle) * 20.0f);
        if (bar < 1) bar = 1;
        if (bar > 20) bar = 20;
        vp.growth_bar = bar;

        // Cap at 99 days for display
        if (vp.growth_day > 99) {
            vp.growth_day = 99;
        }
        
        snprintf(
            vp.growth_str,
            sizeof(vp.growth_str),
            "%d%s",
            vp.growth_day,
            ordinal(vp.growth_day)
        );
        vp_sync_item(VP_GROWTH_STR, &vp.growth_str);
    }
}

// === HMI Initialization ===
void hmi_init(void) {
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
                    // Prevent division by zero
                    if (vp.total_cycle < 1) {
                        vp.total_cycle = 1;
                        vp_sync_item(VP_TOTAL_CYCLE, &vp.total_cycle);
                    }
                    if (vp.growth_day < 1) {
                        vp.growth_day = 1;
                        vp_sync_item(VP_GROWTH_DAY, &vp.growth_day);

                    }

                    vp_growth_bar_update();
                    hmi_update_value(VP_GROWTH_BAR);
                    hmi_update_string(VP_GROWTH_STR);
                    break;
                }

                case VP_WATER_INTERVAL_HR: {
                    if (vp.water_interval_hr < 1) {
                        vp.water_interval_hr = 1;
                        vp_sync_item(VP_WATER_INTERVAL_HR, &vp.water_interval_hr);

                    } else if (vp.water_interval_hr > 12) {
                        vp.water_interval_hr = 12;
                        vp_sync_item(VP_WATER_INTERVAL_HR, &vp.water_interval_hr);
                    }
                    break;
                }

                case VP_WATER_DURATION_SEC: {
                    if (vp.water_duration_sec < 1) {
                        vp.water_duration_sec = 1;
                        vp_sync_item(VP_WATER_DURATION_SEC, &vp.water_duration_sec);

                    } else if (vp.water_duration_sec > 99) {
                        vp.water_duration_sec = 99;
                        vp_sync_item(VP_WATER_DURATION_SEC, &vp.water_duration_sec);
                    }
                    break;
                }

                case VP_LIGHT_AUTO:
                    debug_print("[HMI] Light auto setting changed to ");
                    debug_println(vp.light_auto ? "ENABLED" : "DISABLED");
                    break;

                case VP_WATER_AUTO:
                    debug_print("[HMI] Spray auto setting changed to ");
                    debug_println(vp.water_auto ? "ENABLED" : "DISABLED");
                    break;

                case VP_FAN_AUTO:
                    debug_print("[HMI] Fan auto setting changed to ");
                    debug_println(vp.fan_auto ? "ENABLED" : "DISABLED");
                    break;

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
