#ifndef ESP_NODE_H
#define ESP_NODE_H

// === WiFi Configuration ===
#include <WiFi.h>
#include <WiFiManager.h>
#define WIFI_AP_PSWD "password"
#define WIFI_AP_TIMEOUT 180 // 3 mins 
#define WIFI_STA_MAX_RETRY 15
#define WIFI_STA_RETRY_DELAY 3*60*1000 // 3 mins

// === OTA Configuration ===
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#define OTA_PORT 3232
//#define OTA_PASSWORD "123456"

// === NTP Configuration ===
#include <WiFiUdp.h>
#include <NTPClient.h>
extern NTPClient timeClient;
#define NTP_SERVER "asia.pool.ntp.org"
#define NTP_OFFSET 19800 // UTC+5:30
#define NTP_UPDATE_INTERVAL 30*60*1000 // 30 mins

// === Pin Mapping ===
#define LIGHT_RELAY 23
#define WATER_RELAY 22
#define FAN_RELAY 21
#define RELAY_PIN_4 19

// === Function Prototypes ===
#ifdef __cplusplus
extern "C" {
#endif

void io_init();
uint8_t io_pin_map(uint16_t address);
void io_pin_trigger(
    uint8_t enable, uint8_t current_state,
    uint8_t on_hr, uint8_t on_min,
    uint8_t off_hr, uint8_t off_min,
    uint16_t current_hr, uint16_t current_min,
    uint16_t grace_min, bool on_boot,
    uint16_t address, const char *relay_str
);
void io_pin_trigger_interval_based(
    uint8_t enable, uint8_t current_state,
    uint8_t on_hr, uint8_t on_min,
    uint8_t off_hr, uint8_t off_min,
    uint16_t current_hr, uint16_t current_min,
    uint16_t interval_hr, uint16_t duration_sec,
    uint16_t address, const char *relay_str,
    uint32_t *last_spray
);

void ntp_client_init();
void ntp_client_update(bool force = false);
void hmi_init();
void hmi_on_event(String address, int data, String message, String response);

// === OTA Functions ===
void ota_init();
void ota_mdns_init();

#ifdef __cplusplus
}
#endif

#endif // ESP_NODE_H
