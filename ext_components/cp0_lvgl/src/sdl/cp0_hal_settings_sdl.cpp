#include "hal/hal_settings.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

hal_battery_info_t hal_battery_read(void)
{
    hal_battery_info_t info;
    memset(&info, 0, sizeof(info));
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    constexpr int kMinSoc = 55;
    constexpr int kMaxSoc = 96;
    constexpr int kRange = kMaxSoc - kMinSoc;
    const long tick_100ms = now.tv_sec * 10L + now.tv_nsec / 100000000L;
    const int step = static_cast<int>(tick_100ms % (kRange * 2));
    const int soc = (step <= kRange) ? (kMaxSoc - step) : (kMinSoc + step - kRange);

    info.voltage_mv = 3300 + soc * 9;
    info.current_ma = soc < 50 ? 200 : -200;
    info.temperature_c10 = 350;
    info.soc = soc;
    info.remain_mah = soc * 30;
    info.full_mah = 3000;
    info.flags = soc < 50 ? 1 : 0;
    info.avg_current_ma = info.current_ma;
    info.valid = 1;
    return info;
}

int  hal_backlight_read(void)           { return 75; }
int  hal_backlight_max(void)            { return 100; }
int  hal_backlight_write(int val)       { (void)val; return val; }

int  hal_volume_read(void)              { return 39; }
int  hal_volume_write(int val)          { (void)val; return val; }

hal_wifi_status_t hal_wifi_get_status(void)
{
    hal_wifi_status_t st;
    memset(&st, 0, sizeof(st));
    st.connected = 1;
    strncpy(st.ssid, "SimulatedWiFi", WIFI_SSID_MAX - 1);
    strncpy(st.ip, "192.168.1.100", sizeof(st.ip) - 1);
    st.signal = 80;
    return st;
}

int hal_wifi_scan(hal_wifi_ap_t *out, int max_aps)
{
    if (max_aps < 3) return 0;
    memset(out, 0, sizeof(hal_wifi_ap_t) * 3);
    strncpy(out[0].ssid, "SimulatedWiFi", WIFI_SSID_MAX - 1);
    out[0].signal = 80; strncpy(out[0].security, "WPA2", 31); out[0].in_use = 1;
    strncpy(out[1].ssid, "Neighbor_5G", WIFI_SSID_MAX - 1);
    out[1].signal = 55; strncpy(out[1].security, "WPA2", 31);
    strncpy(out[2].ssid, "FreeWiFi", WIFI_SSID_MAX - 1);
    out[2].signal = 30; strncpy(out[2].security, "Open", 31);
    return 3;
}

int hal_wifi_connect(const char *ssid, const char *password)
{
    (void)ssid; (void)password; return 0;
}

int hal_wifi_disconnect(void) { return 0; }

hal_bt_status_t hal_bt_get_status(void)
{
    hal_bt_status_t st;
    memset(&st, 0, sizeof(st));
    st.powered = 0;
    strncpy(st.address, "00:00:00:00:00:00", sizeof(st.address) - 1);
    return st;
}

int hal_bt_set_power(int on) { (void)on; return 0; }
int hal_bt_scan(hal_bt_device_t *out, int max_devices) { (void)out; (void)max_devices; return 0; }

void hal_time_str(char *buf, int buf_size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, buf_size, "%02d:%02d", t->tm_hour, t->tm_min);
}
