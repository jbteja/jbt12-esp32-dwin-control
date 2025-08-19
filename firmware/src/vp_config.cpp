#include "vp_config.h"

// === GLOBAL VARIABLES ===
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
            return (const char*)vp_items[i].storage_ptr;
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
    for (size_t i = 0; i < num_vp_items; i++) {
        const vp_item_t& item = vp_items[i];
        if (item.address == address) {
            bool changed = false;

            if (item.type == VP_UINT8) {
                uint8_t current = *((uint8_t*)item.storage_ptr);
                uint8_t incoming = *((uint8_t*)new_value);
                if (current != incoming) {
                    *((uint8_t*)item.storage_ptr) = incoming;
                    changed = true;
                }

            } else if (item.type == VP_STRING) {
                const char* new_str = (const char*)new_value;
                char* stored_str = (char*)item.storage_ptr;
                if (strncmp(stored_str, new_str, item.storage_size) != 0) {
                    strncpy(stored_str, new_str, item.storage_size);
                    stored_str[item.storage_size - 1] = '\0';
                    changed = true;
                }
            }

            if (changed) {
                vp_save_item(item);
            }

            return changed;
        }
    }

    return false;  // Not found
}
