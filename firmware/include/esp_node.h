#ifndef ESP_NODE_H
#define ESP_NODE_H

// === WiFi Configuration ===
#include <WiFi.h>
#include <WiFiManager.h>
#define WIFI_AP_PSWD "password"
#define WIFI_STA_MAX_RETRY 15
#define WIFI_STA_RETRY_DELAY 3*60*1000 // 3 minutes

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
#define NTP_UPDATE_INTERVAL 60*60*1000 // 1 hour

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
void ntp_client_init();
void ntp_client_update();
void hmi_init();
void hmi_on_event(String address, int data, String message, String response);

// === OTA Functions ===
void ota_init();
void ota_mdns_init();

#ifdef __cplusplus
}
#endif

#endif // ESP_NODE_H
