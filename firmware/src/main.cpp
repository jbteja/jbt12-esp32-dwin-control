#include "global.h"

void setup() {
    hmi.restartHMI();
    debug_begin(115200);
    debug_println("[BOOT] Initializing communication...");
    delay(1000);

    // Create mutex for VP access
    xVPMutex = xSemaphoreCreateMutex();
    if (xVPMutex == NULL) {
        debug_println("[ERROR] Failed to create VP mutex");
        while(1); // Hang on error
    }

    // Initialize DWIN
    hmi.echoEnabled(false);
    hmi.hmiCallBack(hmi_on_event);
    delay(500);

    // Create queue for HMI updates
    xHMIUpdateQueue = xQueueCreate(15, sizeof(hmi_update_item_t));
    if (xHMIUpdateQueue == NULL) {
        debug_println("[ERROR] Failed to create HMI update queue");
        while(1); // Hang on error
    }

    // Load VP values from NVS and save defaults
    if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
        vp_load_values();

        // Set default placeholder texts
        vp_set_string(VP_HOLDER_SSID, "Network (SSID)");
        vp_set_string(VP_HOLDER_IP, "IP Address");
        vp_set_string(VP_HOLDER_SIGNAL, "Signal Strength");
        vp_set_string(VP_HOLDER_HOSTNAME, "Device ID");
        vp_set_string(VP_HOLDER_UI_VER, "UI Version");
        vp_set_string(VP_HOLDER_FW_VER, "FW Version");
        vp_set_string(VP_HOLDER_HW_VER, "HW Version");

        // Set defaults if empty or changed
        vp_set_string(VP_IP_ADDRESS, "0.0.0.0");
        vp_set_string(VP_PSWD_AND_SIGNAL, "Disconnected");
        vp_set_string(VP_TIME, "00:00");

        // Hostname when not set during production
        if (strlen(vp.hostname) == 0) {
            uint64_t mac = ESP.getEfuseMac();
            uint32_t mac_tail = (uint32_t)(mac & 0xFFFF);
            snprintf(vp.hostname, sizeof(vp.hostname), "E-%04X", mac_tail);
            vp_set_string(VP_HOSTNAME, vp.hostname);
        }

        // Version info
        if (strlen(vp.ui_version) == 0 || 
            strcmp(vp.ui_version, UI_VERSION) != 0
        ) {
            vp_set_string(VP_UI_VERSION, UI_VERSION);
        }

        if (strlen(vp.fw_version) == 0 ||
            strcmp(vp.fw_version, FW_VERSION) != 0
        ) {
            vp_set_string(VP_FW_VERSION, FW_VERSION);
        }

        if (strlen(vp.hw_version) == 0 || 
            strcmp(vp.hw_version, HW_VERSION) != 0
        ) {
            vp_set_string(VP_HW_VERSION, HW_VERSION);
        }

        // If no SSID saved, enable AP mode by default
        if (vp.wifi_ssid[0] == '\0') {
            vp.wifi_ap_state = 1;
        }

        // Default schedules
        if ((vp.light_on_hr == 0) && (vp.light_off_hr == 0)) {
            vp.light_auto = 0;
            vp.light_on_hr = 9;
            vp.light_on_min = 0;
            vp.light_off_hr = 21;
            vp.light_off_min = 0;
        }

        if ((vp.water_on_hr == 0) && (vp.water_off_hr == 0)) {
            vp.water_auto = 0;
            vp.water_on_hr = 9;
            vp.water_on_min = 0;
            vp.water_off_hr = 18;
            vp.water_off_min = 0;
            vp.water_interval_hr = 3;
            vp.water_duration_sec = 30;
        }

        if ((vp.fan_on_hr == 0) && (vp.fan_off_hr == 0)) {
            vp.fan_auto = 0;
            vp.fan_on_hr = 12;
            vp.fan_on_min = 0;
            vp.fan_off_hr = 21;
            vp.fan_off_min = 0;
        }

        // Default growth cycle
        if ((vp.total_cycle == 0) && (vp.growth_day == 0)) {
            vp.plant_id = 0;
            vp.growth_day = 1;
            vp.total_cycle = 15; // Default 15 days 
            vp_growth_bar_update();
        }

        // Test defaults & Simulated values
        // vp.wifi_state = 1;
        // vp.wifi_ap_state = 1;

        vp_save_values();
        xSemaphoreGive(xVPMutex);
    }

    // Print initial values
    debug_printf("[BOOT] Hostname: %s\n", vp.hostname);
    debug_printf("[BOOT] UI Version: %s\n", vp.ui_version);
    debug_printf("[BOOT] FW Version: %s\n", vp.fw_version);
    debug_printf("[BOOT] HW Version: %s\n", vp.hw_version);
    debug_printf(
        "[BOOT] WiFi STA: %d, AP: %d\n",
        vp.wifi_state,
        vp.wifi_ap_state
    );
    debug_printf("[BOOT] WiFi SSID: %s\n", vp.wifi_ssid);
    debug_printf(
        "[BOOT] Light: %d, Water: %d, Fan: %d\n",
        vp.light_state,
        vp.water_state,
        vp.fan_state
    );

    // Initialize IO pins and HMI
    io_init();
    hmi_init();

    // Explicitly set, esp defaults to STA+AP
    WiFi.mode(WIFI_STA);
    
    // Create tasks with core affinity
    xTaskCreatePinnedToCore(
        TaskHMI,            // Task function
        "HMI_Task",         // Task name
        4096,               // Stack size
        NULL,               // Parameters
        TASK_PRIORITY_HMI,  // Priority
        &xHMITaskHandle,    // Task handle
        1                   // Core ID (Core 1)
    );

    xTaskCreatePinnedToCore(
        TaskWiFi,
        "WiFi_Task",
        4096,
        NULL,
        TASK_PRIORITY_WIFI,
        &xWiFiTaskHandle,
        0                  // Core ID (Core 0)
    );

    xTaskCreatePinnedToCore(
        TaskSync,
        "Sync_Task",
        4096,
        NULL,
        TASK_PRIORITY_SYNC,
        &xSyncTaskHandle,
        1               // Core ID (Core 1)
    );

    debug_println("[BOOT] Tasks created. Setup complete!");

    // Delete the setup task as it's no longer needed
    vTaskDelete(NULL);
}

void loop() {
    // Empty - everything is handled by tasks
}
