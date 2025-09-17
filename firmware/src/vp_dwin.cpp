#include "global.h"
#include "vp_dwin.h"

// === DWIN HMI Initialization ===
DWIN hmi(DGUS_SERIAL, 16, 17, DGUS_BAUD); // Serial2 16 as Rx and 17 as Tx

// === Global Instance ===
vp_values_t vp;
Preferences prefs;

// === Load from NVS ===
void vp_load_values() {
    prefs.begin(NVS_NAMESPACE, true);  // Read-only

    // Load each item from NVS
    for (size_t i = 0; i < num_vp_items; i++) {
        const vp_item_t& item = vp_items[i];
        char key[8];
        snprintf(key, sizeof(key), "%04X", item.address);  // Use VP address as key

        if (item.type == VP_UINT8) {
            uint8_t val = prefs.getUChar(key, 0);  // Default to 0 if not found
            *((uint8_t*)item.storage_ptr) = val;

        } else if (item.type == VP_STRING) {
            prefs.getString(key, (char*)item.storage_ptr, item.storage_size);

            // Ensure string is at least null-terminated
            ((char*)item.storage_ptr)[item.storage_size - 1] = '\0';
        }
    }
    
    prefs.end();
}

// === Save to NVS ===
void vp_save_values() {
  prefs.begin(NVS_NAMESPACE, false);  // Read-write

  for (size_t i = 0; i < num_vp_items; i++) {
    const vp_item_t& item = vp_items[i];
    char key[8];
    snprintf(key, sizeof(key), "%04X", item.address);

    if (item.type == VP_UINT8) {
      prefs.putUChar(key, *((uint8_t*)item.storage_ptr));
      
    } else if (item.type == VP_STRING) {
      prefs.putString(key, (const char*)item.storage_ptr);
    }
  }

  prefs.end();
}

// === Get uint8_t value by address ===
uint8_t vp_get_value(uint16_t address) {
    for (size_t i = 0; i < num_vp_items; i++) {
        if (vp_items[i].address == address && vp_items[i].type == VP_UINT8) {
            return *((uint8_t*)vp_items[i].storage_ptr);
        }
    }

    return 0;  // Default if not found
}

// === Set uint8_t value by address ===
bool vp_set_value(uint16_t address, uint8_t value) {
    for (size_t i = 0; i < num_vp_items; i++) {
        if (vp_items[i].address == address && vp_items[i].type == VP_UINT8) {
            *((uint8_t*)vp_items[i].storage_ptr) = value;
            return true;
        }
    }

    return false;
}

// === Get string value by address ===
const char* vp_get_string(uint16_t address) {
    for (size_t i = 0; i < num_vp_items; i++) {
        if (vp_items[i].address == address && vp_items[i].type == VP_STRING) {
            return (char*)vp_items[i].storage_ptr;
        }
    }

    return NULL;  // Not found
}

// === Set string value by address ===
bool vp_set_string(uint16_t address, const char* value) {
    for (size_t i = 0; i < num_vp_items; i++) {
        if (vp_items[i].address == address && vp_items[i].type == VP_STRING) {
            strncpy((char*)vp_items[i].storage_ptr, value, vp_items[i].storage_size);
            ((char*)vp_items[i].storage_ptr)[vp_items[i].storage_size - 1] = '\0';  // Ensure null-termination
            return true;
        }
    }

    return false;
}

// === Save a single item to NVS ===
void vp_save_item(const vp_item_t& item) {
    prefs.begin(NVS_NAMESPACE, false);
    char key[8];
    snprintf(key, sizeof(key), "%04X", item.address);

    if (item.type == VP_UINT8) {
        prefs.putUChar(key, *((uint8_t*)item.storage_ptr));
    
    } else if (item.type == VP_STRING) {
        prefs.putString(key, (const char*)item.storage_ptr);
    }

    prefs.end();
}

// === Synchronize a single item with NVS ===
bool vp_sync_item(uint16_t address, const void* new_value) {
    static uint16_t last_addr = 0;
    static size_t last_index = 0;
    
    // Try last known position first
    size_t start_idx = (address == last_addr) ? last_index : 0;
    
    // Search for item
    const vp_item_t* item = nullptr;
    for (size_t i = start_idx; i < num_vp_items; i++) {
        if (vp_items[i].address == address) {
            item = &vp_items[i];
            last_addr = address;
            last_index = i;
            break;
        }
    }
    
    if (!item) return false;

    bool changed = false;
    
    if (item->type == VP_UINT8) {
        uint8_t current = *((uint8_t*)item->storage_ptr);
        uint8_t incoming = *((uint8_t*)new_value);
        if (current != incoming) {
            *((uint8_t*)item->storage_ptr) = incoming;
            changed = true;
        }
    
    } else if (item->type == VP_STRING) {
        const char* new_str = (const char*)new_value;
        char* stored_str = (char*)item->storage_ptr;

        if (!new_str) return false; // null guard
        
        size_t new_len = strlen(new_str);
        if (new_len >= item->storage_size) {
            // String too long, truncate
            memcpy(stored_str, new_str, item->storage_size - 1);
            stored_str[item->storage_size - 1] = '\0';
            changed = true;
        
        } else if (strcmp(stored_str, new_str) != 0) {
            strcpy(stored_str, new_str);
            changed = true;
        }
    }

    if (changed) {
        vp_save_item(*item);
    }

    return changed;
}

// === Queue HMI update for a value ===
void hmi_update_value(uint16_t address) {
    hmi_update_item_t msg = {
        .type = HMI_UPDATE_VALUE,
        .address = address
    };

    // Queue the update
    BaseType_t xStatus = xQueueSend(xHMIUpdateQueue, &msg, portMAX_DELAY);

    if (xStatus != pdPASS) {
        debug_printf(
            "[ERROR] Failed to queue HMI value update for address 0x%04X\n",
            address
        );
    }
}

// === Queue HMI update for a text field ===
void hmi_update_string(uint16_t address) {
    hmi_update_item_t msg = {
        .type = HMI_UPDATE_STRING,
        .address = address
    };

    // Queue the update
    BaseType_t xStatus = xQueueSend(xHMIUpdateQueue, &msg, portMAX_DELAY);

    if (xStatus != pdPASS) {
        debug_printf(
            "[ERROR] Failed to queue HMI string update for address 0x%04X\n",
            address
        );
    }
}

// === Queue full HMI refresh ===
void hmi_update_all() {
    hmi_update_item_t msg = {
        .type = HMI_UPDATE_ALL,
        .address = 0
    };

    // Queue the update
    BaseType_t xStatus = xQueueSend(xHMIUpdateQueue, &msg, portMAX_DELAY);

    if (xStatus != pdPASS) {
        debug_println("[ERROR] Failed to queue full HMI refresh");
    }
}
