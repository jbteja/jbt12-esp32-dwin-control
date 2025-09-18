#ifndef GLOBAL_COMMON_H
#define GLOBAL_COMMON_H

#include <Arduino.h>
#include "vp_dwin.h"
#include "esp_task.h"
#include "esp_node.h"

// === Device Configuration ===
#define HW_VERSION "v1.0.0"
#define FW_VERSION "v1.0.5"

// === Debugging Macros ===
#define DEBUG_ENABLED 1 // Set 0 for production

#ifdef DEBUG_ENABLED
  #define debug_begin(baud) Serial.begin(baud)
  #define debug_print(x)    Serial.print(x)
  #define debug_println(x)  Serial.println(x)
  #define debug_printf(...) Serial.printf(__VA_ARGS__)
#else
  #define debug_begin(baud)
  #define debug_print(x)
  #define debug_println(x)
  #define debug_printf(...)
#endif

#endif // GLOBAL_COMMON_H
