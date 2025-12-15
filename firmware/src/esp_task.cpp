#include "global.h"

// Task handles
TaskHandle_t xHMITaskHandle = NULL;
TaskHandle_t xWiFiTaskHandle = NULL;
TaskHandle_t xSyncTaskHandle = NULL;

// Mutex for shared resources
SemaphoreHandle_t xVPMutex = NULL;

// Queue for HMI updates
QueueHandle_t xHMIUpdateQueue = NULL;

// Events
EventGroupHandle_t eventGroup = xEventGroupCreate();
const EventBits_t WIFI_CONNECTED_BIT = BIT0;

// === HMI Task ===
void TaskHMI(void *pvParameters) {
    debug_printf("[HMI] Task started on core %d\n", xPortGetCoreID());

    // Wait for initialization to complete
    while (xVPMutex == NULL || xHMIUpdateQueue == NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    hmi_update_item_t msg;
    TickType_t last_listen_time = xTaskGetTickCount();
    const TickType_t listen_interval = pdMS_TO_TICKS(10); // Listen every 10ms

    for (;;) {
        TickType_t current_time = xTaskGetTickCount();

        // Process incoming HMI data
        if ((current_time - last_listen_time) >= listen_interval) {
            hmi.listen();
            last_listen_time = current_time;
        }

        // Process queued HMI updates
        while (xQueueReceive(xHMIUpdateQueue, &msg, 0) == pdTRUE) {
            if (msg.type == HMI_UPDATE_VALUE) {
                uint8_t val = 0;
                if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    val = vp_get_value(msg.address);
                    xSemaphoreGive(xVPMutex);
                } 
                
                // Update HMI display
                hmi.setVP(msg.address, val);
                
                // Control relay if pin is assigned
                uint8_t pin = io_pin_map(msg.address);
                if (pin != 0) {
                    digitalWrite(pin, val);
                }
                
                // Short delay to process
                vTaskDelay(pdMS_TO_TICKS(30));
            
            } else if (msg.type == HMI_UPDATE_STRING) {
                const char* str = "";
                size_t maxlen = 0;
                if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    str = vp_get_string(msg.address);
                    
                    // Find the storage_size for this address
                    for (size_t i = 0; i < num_vp_items; i++) {
                        if (vp_items[i].address == msg.address && 
                            vp_items[i].type == VP_STRING) {
                            maxlen = vp_items[i].storage_size;
                            break;
                        }
                    }
                    xSemaphoreGive(xVPMutex);
                }
                        
                // Create padded string
                String padded_str;
                if (str && maxlen > 0) {
                    size_t real_len = strnlen(str, maxlen);
                    padded_str.reserve(maxlen);
                    padded_str = String(str).substring(0, real_len);
                    while (padded_str.length() < maxlen) {
                        padded_str += ' ';
                    }

                } else if (maxlen > 0) {
                    // Send blank string with full padding
                    padded_str = String(maxlen, ' ');
                
                } else {
                    padded_str = ""; // No padding if maxlen is 0
                }
                
                // Update HMI display
                hmi.setText(msg.address, padded_str);
                
                // Short delay to process
                vTaskDelay(pdMS_TO_TICKS(30));

            } else if (msg.type == HMI_UPDATE_ALL) {
                debug_println("[HMI] Processing full update request");
                
                // Update all VP items
                for (size_t i = 0; i < num_vp_items; i++) {
                    if (vp_items[i].type == VP_UINT8) {
                        uint8_t val = 0;
                        if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                            val = vp_get_value(vp_items[i].address);
                            xSemaphoreGive(xVPMutex);
                        }
                        hmi.setVP(vp_items[i].address, val);
                        
                        // Control relay if pin is assigned
                        uint8_t pin = io_pin_map(vp_items[i].address);
                        if (pin != 0) {
                            digitalWrite(pin, val);
                        }

                    } else if (vp_items[i].type == VP_STRING) {
                        const char* str = "";
                        size_t maxlen = vp_items[i].storage_size;
                        if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                            str = vp_get_string(vp_items[i].address);
                            xSemaphoreGive(xVPMutex);
                        }
                        
                        // Create padded string
                        String padded_str;
                        if (str && maxlen > 0) {
                            size_t real_len = strnlen(str, maxlen);
                            padded_str.reserve(maxlen);
                            padded_str = String(str).substring(0, real_len);
                            while (padded_str.length() < maxlen) {
                                padded_str += ' ';
                            }

                        } else if (maxlen > 0) {
                            // Send blank string with full padding
                            padded_str = String(maxlen, ' ');
                        
                        } else {
                            padded_str = ""; // No padding if maxlen is 0
                        }
                        
                        hmi.setText(vp_items[i].address, padded_str);
                    }

                    // Short delay between updates
                    vTaskDelay(pdMS_TO_TICKS(30));
                }
            } else {
                debug_printf("[HMI] Unknown update type: %d\n", msg.type);
            }
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// === Network Task ===
void TaskWiFi(void *pvParameters) {
    debug_printf("[WiFi] Task started on core %d\n", xPortGetCoreID());
    
    WiFiManager wm;
    int retries = 0;
    bool ap_mode_active = false;
    bool wifi_connected = false;
    static TickType_t last_ntp_sync = 0;
    static TickType_t last_wifi_check = 0;
    
    // Configure WiFiManager
    wm.setDebugOutput(false);
    wm.setConfigPortalTimeout(WIFI_AP_TIMEOUT);
    
    // Custom menu for WiFiManager
    std::vector<const char *> menu = {"wifi", "wifinoscan", "exit"};
    wm.setMenu(menu);

    // Set custom HTML for the configuration portal
    const char* customHeadElement = R"rawliteral(
      <style>
        #hostname, label[for="hostname"], input[name="hostname"] { display: none !important; }
      </style>
    )rawliteral";
    wm.setCustomHeadElement(customHeadElement);

    for (;;) {
        // Check if AP mode should be activated
        if (vp.wifi_ap_state && !ap_mode_active) {
            debug_println("[WiFi] Enabling AP mode for configuration");
            
            // Enter AP mode
            WiFi.disconnect(true);
            WiFi.mode(WIFI_AP);
            ap_mode_active = true;
            
            // Clear WiFi connected event
            wifi_connected = false;
            xEventGroupClearBits(eventGroup, WIFI_CONNECTED_BIT);

            // HMI AP mode parameters
            if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                vp_set_string(VP_HOLDER_SIGNAL, "Password"); // For HMI
                vp_set_string(VP_PSWD_AND_SIGNAL, WIFI_AP_PSWD);
                vp_set_string(VP_IP_ADDRESS, WiFi.softAPIP().toString().c_str());
                vp_save_values();
                xSemaphoreGive(xVPMutex);
            }
            
            // Update HMI
            // hmi_update_text(VP_WIFI_SSID);
            // hmi_update_text(VP_IP_ADDRESS);
            // hmi_update_text(VP_HOLDER_SIGNAL);
            //hmi_update_text(VP_PSWD_AND_SIGNAL);
            
            // Start configuration portal
            debug_printf("[WiFi AP] SSID: %s\n", vp.hostname);
            debug_printf("[WiFi AP] Password: %s\n", WIFI_AP_PSWD);
            debug_printf("[WiFi AP] IP Address: %s\n", vp.ip_address);
            
            if (!wm.startConfigPortal(vp.hostname, WIFI_AP_PSWD)) {
                debug_println("[WiFi AP] Config portal timeout, no WiFi configured");
                
                // Reset AP state and return to STA mode
                if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    vp_set_value(VP_WIFI_STATE, 1);
                    vp_set_value(VP_WIFI_AP_STATE, 0);
                    vp_save_values();
                    xSemaphoreGive(xVPMutex);
                }

            } else {
                // Configuration successful - save credentials
                debug_println("[WiFi AP] Saved WiFi credentials");
                
                // Save credentials and exit AP mode
                if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    // Save WiFi credentials
                    vp_set_string(VP_WIFI_SSID, WiFi.SSID().c_str());
                    vp_set_string(VP_WIFI_PSWD, WiFi.psk().c_str());
                    
                    // Reset AP state
                    vp_set_value(VP_WIFI_STATE, 1);
                    vp_set_value(VP_WIFI_AP_STATE, 0);
                    vp_set_string(VP_HOLDER_SIGNAL, "Signal Strength");
                    vp_set_string(VP_PSWD_AND_SIGNAL, "Connected");
                    vp_save_values();
                    xSemaphoreGive(xVPMutex);
                }
            }

            // Exit AP mode
            WiFi.disconnect(true);
            ap_mode_active = false;
            debug_println("[WiFi] Exiting AP mode!");
        }
        
        // WiFi STA mode - Runs on boot & after AP mode is disabled
        if (!ap_mode_active &&
            !wifi_connected &&
            vp.wifi_state &&
            strlen(vp.wifi_ssid) > 0 &&
            last_wifi_check == 0
        ) {
            debug_printf("[WiFi] Attempting to connect to: %s\n", vp.wifi_ssid);
            
            WiFi.mode(WIFI_STA);
            WiFi.begin(vp.wifi_ssid, vp.wifi_pswd);
            
            // Wait for connection with timeout
            while (
                WiFi.status() != WL_CONNECTED &&
                retries < WIFI_STA_MAX_RETRY
            ) {
                // Random delay between 3 and 10 seconds
                uint32_t delay_ms = 3000 + (esp_random() % 7000);
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
                retries++;
                debug_printf("[WiFi] Retries: %d, delay: %u sec\n", retries, delay_ms / 1000);
            }

            // If max retries reached, enter delay period before next attempt
            if (retries >= WIFI_STA_MAX_RETRY) {
                if (last_wifi_check == 0) {
                    last_wifi_check = xTaskGetTickCount();
                    debug_println("[WiFi] Max retries reached, Entering retry delay period!");
                }
            }
            
            // Check connection result
            if (WiFi.status() == WL_CONNECTED) {
                retries = 0;
                wifi_connected = true;
                xEventGroupSetBits(eventGroup, WIFI_CONNECTED_BIT);
                
                // Save IP address
                if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    vp_set_string(VP_IP_ADDRESS, WiFi.localIP().toString().c_str());
                    // TODO: Update signal strength
                    vp_save_values();
                    xSemaphoreGive(xVPMutex);
                }
                debug_printf("[WiFi] Connected! IP Address: %s\n", vp.ip_address);

                // Initialize WiFi-dependent services
                ota_init();
                ota_mdns_init();
                ntp_client_init();
                ntp_client_update(true);

                // TODO: Update HMI

            } else {
                debug_println("[WiFi] Failed to connect to WiFi!!");
                WiFi.disconnect(true);
                wifi_connected = false;
                xEventGroupClearBits(eventGroup, WIFI_CONNECTED_BIT);
            }
        }
            
        // Handle WiFi connection maintenance
        if (wifi_connected) {
            // Check if WiFi is still connected
            if (WiFi.status() != WL_CONNECTED) {
                debug_println("[WiFi] Disconnected from WiFi");

                // Clear WiFi connected event
                wifi_connected = false;
                xEventGroupClearBits(eventGroup, WIFI_CONNECTED_BIT);
                
                // Save disconnection status
                if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    vp_set_string(VP_IP_ADDRESS, "0.0.0.0");
                    vp_set_string(VP_PSWD_AND_SIGNAL, "Disconnected");
                    vp_save_values();
                    xSemaphoreGive(xVPMutex);
                }

                // TODO: Update HMI
            } else {
                // Handle OTA updates and other WiFi services
                ArduinoOTA.handle();
                
                // Periodic NTP sync
                if ((xTaskGetTickCount() - last_ntp_sync) > pdMS_TO_TICKS(NTP_UPDATE_INTERVAL)) {
                    ntp_client_update();
                    last_ntp_sync = xTaskGetTickCount();
                }
            }
        }

        // Reset retries after delay
        if (last_wifi_check != 0 &&
            (xTaskGetTickCount() - last_wifi_check) > pdMS_TO_TICKS(WIFI_STA_RETRY_DELAY)
        ) {
            retries = 0;
            last_wifi_check = 0;
            debug_println("[WiFi] Retry delay elapsed, Trying to reconnect");
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// === Scheduler / Sync Task ===
void TaskSync(void *pvParameters) {
    debug_printf("[SYNC] Task started on core %d\n", xPortGetCoreID());
    
    static TickType_t current_time;
    static bool on_boot = true;
    static uint32_t last_spray_time = 0;
    const uint16_t grace_period_min = 1; // Error grace period
    
    // Timer tracking for periodic operations
    static TickType_t last_auto_check = 0;
    static TickType_t last_time_check = 0;
    static uint16_t last_auto_minute = -1;
    
    // Run automation checks more frequently for lower latency
    const TickType_t AUTOMATION_INTERVAL = pdMS_TO_TICKS(500);    // 500 ms
    const TickType_t TIME_UPDATE_INTERVAL = pdMS_TO_TICKS(5000);   // 5 secs

    for (;;) {
        current_time = xTaskGetTickCount();

        // Automation Checks (every interval)
        if ((current_time - last_auto_check) >= AUTOMATION_INTERVAL) {
            last_auto_check = current_time;

            // Get current time
            int raw_hours = timeClient.getHours();
            int raw_minutes = timeClient.getMinutes();

            // Validate time before proceeding
            if (raw_hours >= 0 && 
                raw_hours <= 23 && 
                raw_minutes >= 0 && 
                raw_minutes <= 59) {
                uint16_t hours = (uint16_t)raw_hours;
                uint16_t minutes = (uint16_t)raw_minutes;

                // Only run once per minute
                if ((minutes != last_auto_minute) && 
                    (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE)) {
                    last_auto_minute = minutes;

                    // Light automation
                    if (vp.light_auto) {
                        io_pin_trigger(
                            vp.light_auto, vp.light_state,
                            vp.light_on_hr, vp.light_on_min,
                            vp.light_off_hr, vp.light_off_min,
                            hours, minutes,
                            grace_period_min, on_boot,
                            VP_LIGHT_STATE, "Light"
                        );
                    }

                    // Fan automation
                    if (vp.fan_auto) {
                        io_pin_trigger(
                            vp.fan_auto, vp.fan_state,
                            vp.fan_on_hr, vp.fan_on_min,
                            vp.fan_off_hr, vp.fan_off_min,
                            hours, minutes,
                            grace_period_min, on_boot,
                            VP_FAN_STATE, "Fan"
                        );
                    }

                    xSemaphoreGive(xVPMutex);
                }

                // Spray automation runs every cycle
                if (vp.water_auto &&
                    xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    io_pin_trigger_interval_based(
                        vp.water_auto, vp.water_state,
                        vp.water_on_hr, vp.water_on_min,
                        vp.water_off_hr, vp.water_off_min,
                        hours, minutes,
                        vp.water_interval_hr, vp.water_duration_sec,
                        VP_WATER_STATE, "Spray",
                        &last_spray_time
                    );

                    xSemaphoreGive(xVPMutex);
                }

                // Handle boot flag (one-time operation)
                if (on_boot) {
                    on_boot = false;
                    debug_println("[SYNC] Boot automation check completed");
                }

            } else {
                debug_printf("[SYNC] Invalid time: %d:%d\n", raw_hours, raw_minutes);
            }
        }

        // HMI Time Update (every 5 seconds)
        if ((current_time - last_time_check) >= TIME_UPDATE_INTERVAL) {
            last_time_check = current_time;

            // Get current time
            int raw_hours = timeClient.getHours();
            int raw_minutes = timeClient.getMinutes();
            
            if (raw_hours >= 0 &&
                raw_hours <= 23 &&
                raw_minutes >= 0 &&
                raw_minutes <= 59) {
                uint16_t hours = (uint16_t)raw_hours;
                uint16_t minutes = (uint16_t)raw_minutes;

                // Update data to display on HMI
                if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    char time[6] = {0};
                    snprintf(time, sizeof(time), "%02u:%02u", hours, minutes);

                    // Update only if changed
                    const char* cur = vp_get_string(VP_TIME);
                    if ((cur == NULL || strcmp(cur, time) != 0) &&
                        time[0] != '\0') {
                        vp_set_string(VP_TIME, time);
                        vp_save_values();
                        hmi_update_string(VP_TIME);
                    }
                    xSemaphoreGive(xVPMutex);
                }
            }
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
