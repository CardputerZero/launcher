#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_AP_MAX    32
#define WIFI_SSID_MAX  64

typedef struct {
    int voltage_mv;
    int current_ma;
    int temperature_c10;
    int soc;
    int remain_mah;
    int full_mah;
    int flags;
    int avg_current_ma;
    int valid;
} hal_battery_info_t;

typedef struct {
    char ssid[WIFI_SSID_MAX];
    int  signal;
    char security[32];
    int  in_use;
} hal_wifi_ap_t;

typedef struct {
    int  connected;
    char ssid[WIFI_SSID_MAX];
    char ip[48];
    int  signal;
} hal_wifi_status_t;

typedef struct {
    int  powered;
    char address[24];
} hal_bt_status_t;

hal_battery_info_t hal_battery_read(void);

int  hal_backlight_read(void);
int  hal_backlight_max(void);
int  hal_backlight_write(int val);

int  hal_volume_read(void);
int  hal_volume_write(int val);

hal_wifi_status_t hal_wifi_get_status(void);
int  hal_wifi_scan(hal_wifi_ap_t *out, int max_aps);
int  hal_wifi_connect(const char *ssid, const char *password);

#define BT_DEVICE_MAX  16
#define BT_NAME_MAX    64

typedef struct {
    char name[BT_NAME_MAX];
    char address[24];
    int  rssi;
    int  connected;
} hal_bt_device_t;

hal_bt_status_t hal_bt_get_status(void);
int  hal_bt_set_power(int on);
int  hal_bt_scan(hal_bt_device_t *out, int max_devices);

void hal_time_str(char *buf, int buf_size);

#ifdef __cplusplus
}
#endif
