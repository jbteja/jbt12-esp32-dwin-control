#ifndef VP_CONFIG_H
#define VP_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <Preferences.h>

#ifndef NVS_NAMESPACE
#define NVS_NAMESPACE "vp-flash"
#endif

// === VP VALUE TYPES ===
typedef enum {
  VP_UINT8,
  VP_STRING
} vp_type_t;

// === DATA STORAGE STRUCTURE ===
typedef struct {
  char  time_str[6];
  char  hostname[7];
  uint8_t plant_id;
  uint8_t total_cycle;
  uint8_t growth_day;
  uint8_t growth_bar;
  char  growth_str[6];
  char  fw_version[7];
  char  hw_version[7];

  uint8_t light_state;
  uint8_t light_auto;
  uint8_t light_on_hr;
  uint8_t light_on_min;
  uint8_t light_off_hr;
  uint8_t light_off_min;

  uint8_t water_state;
  uint8_t water_auto;
  uint8_t water_on_hr;
  uint8_t water_on_min;
  uint8_t water_off_hr;
  uint8_t water_off_min;
  uint8_t water_interval_hr;
  uint8_t water_duration_sec;

  uint8_t fan_state;
  uint8_t fan_auto;
  uint8_t fan_on_hr;
  uint8_t fan_on_min;
  uint8_t fan_off_hr;
  uint8_t fan_off_min;

  char  wifi_state[16];
  char  wifi_ssid[32];
  char  wifi_password[32];
  char  ip_address[16];
  char  signal_strength[16];
  uint8_t config_network;
} vp_values_t;

// === Global INStance ===
extern vp_values_t vp;

// === VP ITEM STRUCT ===
typedef struct {
  uint16_t address;
  vp_type_t type;
  void* storage_ptr;
  size_t storage_size;
} vp_item_t;

// === MACROS FOR VP ITEMS ===
#define VP_ITEM_UINT8(addr, field)   { \
  addr, VP_UINT8, &vp.field, sizeof(vp.field) \
}
#define VP_ITEM_STRING(addr, field)  { \
  addr, VP_STRING, &vp.field, sizeof(vp.field) \
}

// === VP ITEM ADDRESSES (MACROS) ===
#define VP_TIME               0x1000
#define VP_HOSTNAME           0x1010
#define VP_PLANT_ID           0x1020
#define VP_TOTAL_CYCLE        0x1030
#define VP_GROWTH_DAY         0x1040
#define VP_GROWTH_BAR         0x1050
#define VP_GROWTH_STR         0x1060
#define VP_FW_VERSION         0x1070
#define VP_HW_VERSION         0x1080

#define VP_LIGHT_STATE        0x1100
#define VP_LIGHT_AUTO         0x1110
#define VP_LIGHT_ON_HR        0x1120
#define VP_LIGHT_ON_MIN       0x1130
#define VP_LIGHT_OFF_HR       0x1140
#define VP_LIGHT_OFF_MIN      0x1150

#define VP_WATER_STATE        0x1200
#define VP_WATER_AUTO         0x1210
#define VP_WATER_ON_HR        0x1220
#define VP_WATER_ON_MIN       0x1230
#define VP_WATER_OFF_HR       0x1240
#define VP_WATER_OFF_MIN      0x1250
#define VP_WATER_INTERVAL_HR  0x1260
#define VP_WATER_DURATION_SEC 0x1270

#define VP_FAN_STATE          0x1300
#define VP_FAN_AUTO           0x1310
#define VP_FAN_ON_HR          0x1320
#define VP_FAN_ON_MIN         0x1330
#define VP_FAN_OFF_HR         0x1340
#define VP_FAN_OFF_MIN        0x1350

#define VP_WIFI_STATE         0x1400
#define VP_WIFI_SSID          0x1410
#define VP_WIFI_PASSWORD      0x1420
#define VP_IP_ADDRESS         0x1430
#define VP_SIGNAL_STRENGTH    0x1440
#define VP_CONFIG_NETWORK     0x1450

// === VP ITEM TABLE ===
static const vp_item_t vp_items[] = {
  VP_ITEM_STRING(VP_TIME, time_str),
  VP_ITEM_STRING(VP_HOSTNAME, hostname),
  VP_ITEM_UINT8(VP_PLANT_ID, plant_id),
  VP_ITEM_UINT8(VP_TOTAL_CYCLE, total_cycle),
  VP_ITEM_UINT8(VP_GROWTH_DAY, growth_day),
  VP_ITEM_UINT8(VP_GROWTH_BAR, growth_bar),
  VP_ITEM_STRING(VP_GROWTH_STR, growth_str),
  VP_ITEM_STRING(VP_FW_VERSION, fw_version),
  VP_ITEM_STRING(VP_HW_VERSION, hw_version),

  VP_ITEM_UINT8(VP_LIGHT_STATE, light_state),
  VP_ITEM_UINT8(VP_LIGHT_AUTO, light_auto),
  VP_ITEM_UINT8(VP_LIGHT_ON_HR, light_on_hr),
  VP_ITEM_UINT8(VP_LIGHT_ON_MIN, light_on_min),
  VP_ITEM_UINT8(VP_LIGHT_OFF_HR, light_off_hr),
  VP_ITEM_UINT8(VP_LIGHT_OFF_MIN, light_off_min),

  VP_ITEM_UINT8(VP_WATER_STATE, water_state),
  VP_ITEM_UINT8(VP_WATER_AUTO, water_auto),
  VP_ITEM_UINT8(VP_WATER_ON_HR, water_on_hr),
  VP_ITEM_UINT8(VP_WATER_ON_MIN, water_on_min),
  VP_ITEM_UINT8(VP_WATER_OFF_HR, water_off_hr),
  VP_ITEM_UINT8(VP_WATER_OFF_MIN, water_off_min),
  VP_ITEM_UINT8(VP_WATER_INTERVAL_HR, water_interval_hr),
  VP_ITEM_UINT8(VP_WATER_DURATION_SEC, water_duration_sec),

  VP_ITEM_UINT8(VP_FAN_STATE, fan_state),
  VP_ITEM_UINT8(VP_FAN_AUTO, fan_auto),
  VP_ITEM_UINT8(VP_FAN_ON_HR, fan_on_hr),
  VP_ITEM_UINT8(VP_FAN_ON_MIN, fan_on_min),
  VP_ITEM_UINT8(VP_FAN_OFF_HR, fan_off_hr),
  VP_ITEM_UINT8(VP_FAN_OFF_MIN, fan_off_min),

  VP_ITEM_STRING(VP_WIFI_STATE, wifi_state),
  VP_ITEM_STRING(VP_WIFI_SSID, wifi_ssid),
  VP_ITEM_STRING(VP_WIFI_PASSWORD, wifi_password),
  VP_ITEM_STRING(VP_IP_ADDRESS, ip_address),
  VP_ITEM_STRING(VP_SIGNAL_STRENGTH, signal_strength),
  VP_ITEM_UINT8(VP_CONFIG_NETWORK, config_network)
};

// === COUNT ===
static const size_t num_vp_items = sizeof(vp_items) / sizeof(vp_item_t);

// === FUNCTION PROTOTYPES ===
#ifdef __cplusplus
extern "C" {
#endif

void vp_load_values();
void vp_save_values();
uint8_t vp_get_value(uint16_t address);
bool vp_set_value(uint16_t address, uint8_t value);
const char* vp_get_string(uint16_t address);
bool vp_set_string(uint16_t address, const char* value);
void vp_save_item(const vp_item_t& item);
bool vp_sync_item(uint16_t address, const void* new_value);

#ifdef __cplusplus
}
#endif

#endif // VP_CONFIG_H
