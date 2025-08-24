#include "global.h"

void setup() {
    debug_begin(115200);
    debug_println("[BOOT] Initializing communication...");
    delay(1000);

    // Create mutex for VP access
    xVPMutex = xSemaphoreCreateMutex();
    if (xVPMutex == NULL) {
        debug_println("[ERROR] Failed to create VP mutex");
        while(1); // Hang on error
    }

    // Initialize DWIN HMI
    hmi.echoEnabled(false);
    hmi.hmiCallBack(hmi_on_event);
    delay(500);

    // Load VP values from NVS and save defaults
    if (xSemaphoreTake(xVPMutex, portMAX_DELAY) == pdTRUE) {
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

        vp_save_values();
        xSemaphoreGive(xVPMutex);
    }

    // Print initial values
    debug_printf("[BOOT] Hostname: %s\n", vp.hostname);
    debug_printf("[BOOT] HW Version: %s\n", vp.hw_version);
    debug_printf("[BOOT] FW Version: %s\n", vp.fw_version);
    debug_printf(
        "[BOOT] Light: %d, Water: %d, Fan: %d\n",
        vp.light_state,
        vp.water_state,
        vp.fan_state
    );

    // Initialize IO pins and HMI
    io_init();
    hmi_init();
    
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
        TaskApp,
        "App_Task",
        4096,
        NULL,
        TASK_PRIORITY_APP,
        &xAppTaskHandle,
        1               // Core ID (Core 1)
    );

    debug_println("[BOOT] Tasks created. Scheduler starting...");
    //hmi.beepHMI();

    // Delete the setup task as it's no longer needed
    vTaskDelete(NULL);
}

void loop() {
    // Empty - everything is handled by tasks
}
