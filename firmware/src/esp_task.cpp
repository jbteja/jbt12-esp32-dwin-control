#include "global.h"

// Task handles
TaskHandle_t xHMITaskHandle = NULL;
TaskHandle_t xWiFiTaskHandle = NULL;
TaskHandle_t xAppTaskHandle = NULL;

// Mutex for shared resources
SemaphoreHandle_t xVPMutex = NULL;

// Events
EventGroupHandle_t eventGroup = xEventGroupCreate();
const EventBits_t WIFI_CONNECTED_BIT = BIT0;

// === HMI Task ===
void TaskHMI(void *pvParameters) {
    debug_printf("[HMI] Task started on core %d\n", xPortGetCoreID());

    // Wait for initialization to complete
    while (xVPMutex == NULL) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    for (;;) {
        // Process HMI communication
        hmi.listen();
        
        // Yield to other tasks
        vTaskDelay(1 / portTICK_PERIOD_MS);
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
    wm.setConfigPortalTimeout(180); // 3 minutes timeout
    
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
                vp_set_string(VP_HOLDER_SIGNAL, "Password"); // For showing in HMI
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
            
            if (!wm.startConfigPortal(vp.hostname, WIFI_AP_PSWD)) {
                debug_println("[WiFi AP] Config portal timeout, no WiFi configured");
                
                // Reset AP state and return to STA mode
                if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
                    vp.wifi_state = 1;
                    vp.wifi_ap_state = 0;
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
                    vp.wifi_state = 1;
                    vp.wifi_ap_state = 0;
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
                vTaskDelay(delay_ms / portTICK_PERIOD_MS);
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
                    vp_save_values();
                    xSemaphoreGive(xVPMutex);
                }
                debug_printf("[WiFi] Connected! IP Address: %s\n", vp.ip_address);

                // Initialize WiFi-dependent services
                ota_init();
                ota_mdns_init();
                ntp_client_init();
                ntp_client_update();

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
                if ((xTaskGetTickCount() - last_ntp_sync) > (NTP_UPDATE_INTERVAL / portTICK_PERIOD_MS)) {
                    ntp_client_update();
                    last_ntp_sync = xTaskGetTickCount();
                }
            }
        }

        // Reset retries after delay
        if (last_wifi_check != 0 &&
            (xTaskGetTickCount() - last_wifi_check) > (WIFI_STA_RETRY_DELAY / portTICK_PERIOD_MS)
        ) {
            retries = 0;
            last_wifi_check = 0;
            debug_println("[WiFi] Retry delay elapsed, Trying to reconnect");
        }

        // Small delay to prevent task from hogging CPU
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

//     WiFiManager wm;
//     bool ap_mode_active = false;
//     bool wifi_connected = false;
    
//     // Configure WiFiManager
//     wm.setDebugOutput(false);
//     wm.setConfigPortalTimeout(180); // 3 minutes timeout
//     wm.setSaveConfigCallback([]() {
//         debug_println("[WiFi] Configuration saved, will attempt connection");
//     });
    
//     for (;;) {
//         if (vp.wifi_ap_state && !ap_mode_active) {
//             debug_println("[WiFi] AP mode started");
            
//             WiFiManager wm;
//             ap_mode_active = true;
//             xEventGroupClearBits(eventGroup, WIFI_CONNECTED_BIT);

//             // Set AP mode parameters
//             if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
//                 vp_sync_item(VP_WIFI_PASSWORD, WIFI_AP_PASSWORD);
//                 xSemaphoreGive(xVPMutex);
//             }
            
//             wm.autoConnect(vp.hostname, WIFI_AP_PASSWORD);
//             debug_printf("[WiFi AP] SSID: %s\n", vp.hostname);
//             debug_printf("[WiFi AP] Password: %s\n", WIFI_AP_PASSWORD);
//             debug_printf("[WiFi AP] IP Address: %s\n", WiFi.softAPIP().toString().c_str());
//         } else if (!vp.wifi_ap_state && ap_mode_active) {
//             debug_println("[WiFi] AP mode stopped");
//             ap_mode_active = false;
//         }
//     }

//     ota_init();
//     ota_mdns_init();
//     ntp_client_init();

//     // Flag to track WiFi connection status
//     bool wifi_connected = false;
//     bool ap_mode_active = false;
//     String hostname = String(vp.hostname) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    
//     // Check initial WiFi mode settings
//     if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
//         debug_println("[WiFi] Task started");
        
//         // Check if AP mode is requested
//         if (vp.wifi_ap_state) {
//             debug_println("[WiFi] Starting AP mode");
            
//             // Start WiFi in AP mode
//             WiFi.mode(WIFI_AP);
//             WiFi.softAP(hostname.c_str(), "password123");
//             ap_mode_active = true;
            
//             // Update HMI
//             vp.wifi_sta_state = 0;
//             hmi.setVP(VP_WIFI_STA_STATE, vp.wifi_sta_state);
            
//             // Set IP address display
//             IPAddress ip = WiFi.softAPIP();
//             snprintf(vp.ip_address, sizeof(vp.ip_address), "%d.%d.%d.%d", 
//                     ip[0], ip[1], ip[2], ip[3]);
//             hmi.setVP(VP_IP_ADDRESS, vp.ip_address);
            
//             // Set connection status
//             snprintf(vp.conn_and_signal, sizeof(vp.conn_and_signal), "AP Mode Active");
//             hmi.setVP(VP_CONN_AND_SIGNAL, vp.conn_and_signal);
//         } 
//         // Try to connect to saved WiFi if available
//         else if (strlen(vp.wifi_ssid) > 0) {
//             debug_printf("[WiFi] Connecting to saved network: %s\n", vp.wifi_ssid);
            
//             WiFi.mode(WIFI_STA);
//             WiFi.begin(vp.wifi_ssid, vp.wifi_password);
//         }
        
//         xSemaphoreGive(xVPMutex);
//     }
    
//     // Initialize event group for task synchronization
//     EventGroupHandle_t wifi_event_group = xEventGroupCreate();
//     const int WIFI_CONNECTED_BIT = BIT0;
    
//     // Main task loop
//     for (;;) {
//         // Check WiFi connection status
//         if (WiFi.status() == WL_CONNECTED && !wifi_connected) {
//             wifi_connected = true;
            
//             if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
//                 debug_println("[WiFi] Connected to WiFi!");
                
//                 // Update connection status
//                 vp.wifi_sta_state = 1;
//                 hmi.setVP(VP_WIFI_STA_STATE, vp.wifi_sta_state);
                
//                 // Save current WiFi credentials if we were in AP mode
//                 if (ap_mode_active) {
//                     strncpy(vp.wifi_ssid, WiFi.SSID().c_str(), sizeof(vp.wifi_ssid));
//                     strncpy(vp.wifi_password, WiFi.psk().c_str(), sizeof(vp.wifi_password));
//                     vp.wifi_ssid[sizeof(vp.wifi_ssid) - 1] = '\0';
//                     vp.wifi_password[sizeof(vp.wifi_password) - 1] = '\0';
//                     vp.wifi_ap_state = 0;
//                     hmi.setVP(VP_WIFI_AP_STATE, vp.wifi_ap_state);
//                     vp_save_values();
//                     ap_mode_active = false;
//                 }
                
//                 // Set IP address display
//                 IPAddress ip = WiFi.localIP();
//                 snprintf(vp.ip_address, sizeof(vp.ip_address), "%d.%d.%d.%d", 
//                         ip[0], ip[1], ip[2], ip[3]);
//                 hmi.setVP(VP_IP_ADDRESS, vp.ip_address);
                
//                 // Set connection status with signal strength
//                 int rssi = WiFi.RSSI();
//                 snprintf(vp.conn_and_signal, sizeof(vp.conn_and_signal), "RSSI: %d dBm", rssi);
//                 hmi.setVP(VP_CONN_AND_SIGNAL, vp.conn_and_signal);
                
//                 xSemaphoreGive(xVPMutex);
                
//                 // Initialize NTP and OTA after WiFi connection
//                 ntp_client_init();
//                 ota_init();
//                 ota_mdns_init();
                
//                 // Set the connected bit to notify other tasks
//                 xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
//             }
//         } 
//         else if (WiFi.status() != WL_CONNECTED && wifi_connected) {
//             wifi_connected = false;
            
//             if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
//                 debug_println("[WiFi] Disconnected from WiFi");
                
//                 // Update connection status
//                 vp.wifi_sta_state = 0;
//                 hmi.setVP(VP_WIFI_STA_STATE, vp.wifi_sta_state);
                
//                 // Clear IP and connection status
//                 snprintf(vp.ip_address, sizeof(vp.ip_address), "0.0.0.0");
//                 hmi.setVP(VP_IP_ADDRESS, vp.ip_address);
                
//                 snprintf(vp.conn_and_signal, sizeof(vp.conn_and_signal), "Disconnected");
//                 hmi.setVP(VP_CONN_AND_SIGNAL, vp.conn_and_signal);
                
//                 xSemaphoreGive(xVPMutex);
                
//                 // Clear the connected bit
//                 xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
//             }
//         }
        
//         // Check for AP mode toggle from HMI
//         if (xSemaphoreTake(xVPMutex, 0) == pdTRUE) {
//             // If AP mode is requested but not active
//             if (vp.wifi_ap_state && !ap_mode_active) {
//                 debug_println("[WiFi] AP mode requested from HMI");
                
//                 // Disconnect from current WiFi if connected
//                 if (wifi_connected) {
//                     WiFi.disconnect();
//                     wifi_connected = false;
//                 }
                
//                 // Start AP mode
//                 WiFi.mode(WIFI_AP);
//                 WiFi.softAP(hostname.c_str(), "password123");
//                 ap_mode_active = true;
                
//                 // Update HMI
//                 vp.wifi_sta_state = 0;
//                 hmi.setVP(VP_WIFI_STA_STATE, vp.wifi_sta_state);
                
//                 // Set IP address display
//                 IPAddress ip = WiFi.softAPIP();
//                 snprintf(vp.ip_address, sizeof(vp.ip_address), "%d.%d.%d.%d", 
//                         ip[0], ip[1], ip[2], ip[3]);
//                 hmi.setVP(VP_IP_ADDRESS, vp.ip_address);
                
//                 // Set connection status
//                 snprintf(vp.conn_and_signal, sizeof(vp.conn_and_signal), "AP Mode Active");
//                 hmi.setVP(VP_CONN_AND_SIGNAL, vp.conn_and_signal);
//             }
//             // If AP mode is turned off but active
//             else if (!vp.wifi_ap_state && ap_mode_active) {
//                 debug_println("[WiFi] AP mode disabled from HMI");
                
//                 // Stop AP mode and try to connect to saved WiFi
//                 WiFi.softAPDisconnect(true);
//                 ap_mode_active = false;
                
//                 // Try to connect to saved WiFi if available
//                 if (strlen(vp.wifi_ssid) > 0) {
//                     debug_printf("[WiFi] Connecting to saved network: %s\n", vp.wifi_ssid);
                    
//                     WiFi.mode(WIFI_STA);
//                     WiFi.begin(vp.wifi_ssid, vp.wifi_password);
//                 }
//             }
            
//             xSemaphoreGive(xVPMutex);
//         }
        
//         // Update NTP client periodically when connected
//         static unsigned long last_ntp_update = 0;
//         if (wifi_connected && (millis() - last_ntp_update > NTP_UPDATE_INTERVAL)) {
//             ntp_client_update();
//             last_ntp_update = millis();
//         }
        
//         // Handle OTA updates when connected
//         if (wifi_connected) {
//             ArduinoOTA.handle();
//         }
        
//         // Short delay to prevent task starvation
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//     }
// }




// void TaskWiFi(void *pvParameters) {
//     // Initialize WiFi manager
//     WiFiManagerHandler::init();
    
//     // Load saved WiFi credentials
//     if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
//         if (strlen(vp.wifi_ssid) > 0) {
//             WiFi.begin(vp.wifi_ssid, vp.wifi_password);
//         }
//         xSemaphoreGive(xVPMutex);
//     }
    
//     for (;;) {
//         // Check for WiFi config notification
//         uint32_t notificationValue;
//         if (xTaskNotifyWait(0, ULONG_MAX, &notificationValue, 0) == pdTRUE) {
//             if (notificationValue & NOTIFICATION_WIFI_CONFIG) {
//                 WiFiManagerHandler::startConfigPortal();
//             }
            
//             if (notificationValue & NOTIFICATION_SUSPEND) {
//                 // Suspend task until resumed
//                 ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
//             }
//         }
        
//         // Handle WiFi manager
//         WiFiManagerHandler::handle();
        
//         // Update WiFi status on HMI
//         updateWiFiStatus();
        
//         // Handle OTA updates
//         ArduinoOTA.handle();
        
//         vTaskDelay(10000 / portTICK_PERIOD_MS);
//     }
// }

// void TaskApplication(void *pvParameters) {
//     const TickType_t xFrequency = 1000 / portTICK_PERIOD_MS;
//     TickType_t xLastWakeTime = xTaskGetTickCount();
    
//     for (;;) {
//         // Wait for next cycle
//         vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
//         // Check for suspend notification
//         uint32_t notificationValue;
//         if (xTaskNotifyWait(0, ULONG_MAX, &notificationValue, 0) == pdTRUE) {
//             if (notificationValue & NOTIFICATION_SUSPEND) {
//                 // Suspend task until resumed
//                 ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
//             }
//         }
        
//         // Application logic here
//         // (e.g., update growth bar, check sensors, etc.)
        
//         // Example: Update growth bar periodically
//         if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
//             if (vp.total_cycle > 0) {
//                 uint8_t bar = (uint8_t)round((vp.growth_day / (float)vp.total_cycle) * 20.0f);
//                 if (bar < 1) bar = 1;
//                 if (bar > 20) bar = 20;
//                 vp.growth_bar = bar;
//                 hmi_update_value(VP_GROWTH_BAR);
//             }
//             xSemaphoreGive(xVPMutex);
//         }
//     }
// }

// void updateWiFiStatus() {
//     if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
//         if (WiFi.isConnected()) {
//             snprintf(vp.wifi_status, sizeof(vp.wifi_status), 
//                      "Connected: %s", WiFi.SSID().c_str());
//         } else if (WiFiManagerHandler::isConfigPortalActive()) {
//             snprintf(vp.wifi_status, sizeof(vp.wifi_status), 
//                      "Config Mode: GrowBox_AP");
//         } else {
//             snprintf(vp.wifi_status, sizeof(vp.wifi_status), 
//                      "Disconnected");
//         }
        
//         hmi_update_text(VP_WIFI_STATUS);
//         xSemaphoreGive(xVPMutex);
//     }
// }