#ifndef ESP_NODE_H
#define ESP_NODE_H

#include <Arduino.h>
#include <DWIN.h>

// === Device Configuration ===
#define HW_VERSION "v1.0.0"
#define FW_VERSION "v1.0.2"

// === Pin Mapping ===
#define LIGHT_RELAY 23
#define WATER_RELAY 22
#define FAN_RELAY 21
#define RELAY_PIN_4 19

// === DWIN Configuration ===
#define DGUS_BAUD 115200
#define DGUS_SERIAL Serial2

// === FUNCTION PROTOTYPES ===
#ifdef __cplusplus
extern "C" {
#endif

void io_init();
void hmi_init();
void hmi_update_text(uint16_t address);
void hmi_update_value(uint16_t address, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif // ESP_NODE_H
