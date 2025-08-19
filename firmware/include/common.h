#ifndef GENERIC_COMMON_H
#define GENERIC_COMMON_H

#include <Arduino.h>
#include "esp_node.h"
#include "vp_config.h"

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

#endif /* GENERIC_COMMON_H */