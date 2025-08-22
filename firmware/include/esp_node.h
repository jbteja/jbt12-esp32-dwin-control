#ifndef ESP_NODE_H
#define ESP_NODE_H

// === NTP Configuration ===
#define NTP_SERVER "asia.pool.ntp.org"
#define NTP_OFFSET 19800 // UTC+5:30
#define NTP_UPDATE_INTERVAL 10*60*1000

// === Pin Mapping ===
#define LIGHT_RELAY 23
#define WATER_RELAY 22
#define FAN_RELAY 21
#define RELAY_PIN_4 19

// === FUNCTION PROTOTYPES ===
#ifdef __cplusplus
extern "C" {
#endif

void io_init();
void hmi_init();
void hmi_update_text(uint16_t address);
void hmi_update_value(uint16_t address, uint8_t pin = 0);
void hmi_on_event(String address, int data, String message, String response);

#ifdef __cplusplus
}
#endif

#endif // ESP_NODE_H
