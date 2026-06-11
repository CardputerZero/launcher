#include "cp0_lvgl_app.h"

#include "hal/hal_audio.h"
#include "hal/hal_config.h"
#include "hal/hal_filesystem.h"
#include "hal/hal_network.h"
#include "hal/hal_process.h"
#include "hal/hal_pty.h"
#include "hal/hal_screenshot.h"
#include "hal/hal_settings.h"

#include <cstring>

extern "C" {

void cp0_signal_audio_api_play_file(const char *path)
{
    if (path && path[0]) {
        hal_audio_play(path);
    }
}

void cp0_signal_audio_api_play_asset(const char *name)
{
    const char *path = cp0_file_path_c(name);
    cp0_signal_audio_api_play_file(path && path[0] ? path : name);
}

void cp0_signal_system_play_asset(const char *name)
{
    cp0_signal_audio_api_play_asset(name);
}

void cp0_config_init(void)
{
    hal_config_init();
}

int cp0_config_get_int(const char *key, int default_val)
{
    return hal_config_get_int(key, default_val);
}

void cp0_config_set_int(const char *key, int val)
{
    hal_config_set_int(key, val);
}

const char *cp0_config_get_str(const char *key, const char *default_val)
{
    return hal_config_get_str(key, default_val);
}

void cp0_config_set_str(const char *key, const char *val)
{
    hal_config_set_str(key, val);
}

void cp0_config_save(void)
{
    hal_config_save();
}

int cp0_dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count)
{
    if (!entries || max_entries <= 0) {
        return hal_dir_list(path, nullptr, 0, out_count);
    }

    hal_dirent_t hal_entries[max_entries];
    int ret = hal_dir_list(path, hal_entries, max_entries, out_count);
    int count = out_count ? *out_count : 0;
    if (count > max_entries) {
        count = max_entries;
    }

    for (int i = 0; i < count; ++i) {
        std::strncpy(entries[i].name, hal_entries[i].name, sizeof(entries[i].name) - 1);
        entries[i].name[sizeof(entries[i].name) - 1] = '\0';
        entries[i].is_dir = hal_entries[i].is_dir;
    }

    return ret;
}

cp0_watcher_t cp0_dir_watch_start(const char *path)
{
    return reinterpret_cast<cp0_watcher_t>(hal_dir_watch_start(path));
}

int cp0_dir_watch_poll(cp0_watcher_t watcher)
{
    return hal_dir_watch_poll(reinterpret_cast<hal_watcher_t>(watcher));
}

void cp0_dir_watch_stop(cp0_watcher_t watcher)
{
    hal_dir_watch_stop(reinterpret_cast<hal_watcher_t>(watcher));
}

int cp0_network_list(cp0_netif_info_t *entries, int max_entries, int *out_count)
{
    if (!entries || max_entries <= 0) {
        return hal_network_list(nullptr, 0, out_count);
    }

    hal_netif_info_t hal_entries[max_entries];
    int ret = hal_network_list(hal_entries, max_entries, out_count);
    int count = out_count ? *out_count : 0;
    if (count > max_entries) {
        count = max_entries;
    }

    for (int i = 0; i < count; ++i) {
        std::strncpy(entries[i].iface, hal_entries[i].iface, sizeof(entries[i].iface) - 1);
        entries[i].iface[sizeof(entries[i].iface) - 1] = '\0';
        std::strncpy(entries[i].ipv4, hal_entries[i].ipv4, sizeof(entries[i].ipv4) - 1);
        entries[i].ipv4[sizeof(entries[i].ipv4) - 1] = '\0';
        std::strncpy(entries[i].netmask, hal_entries[i].netmask, sizeof(entries[i].netmask) - 1);
        entries[i].netmask[sizeof(entries[i].netmask) - 1] = '\0';
        entries[i].is_up = hal_entries[i].is_up;
    }

    return ret;
}

int cp0_process_exec_blocking(const char *exec_path, volatile int *home_key_flag, int keep_root)
{
    return hal_process_exec_blocking(exec_path, home_key_flag, keep_root);
}

cp0_pid_t cp0_process_spawn(const char *exec_path, int keep_root)
{
    return hal_process_spawn(exec_path, keep_root);
}

void cp0_process_stop(cp0_pid_t pid)
{
    hal_process_stop(pid);
}

int cp0_process_check_lock(const char *lock_path, int *holder_pid)
{
    return hal_process_check_lock(lock_path, holder_pid);
}

void cp0_process_kill(int pid, int grace_ms)
{
    hal_process_kill(pid, grace_ms);
}

void cp0_system_shutdown(void)
{
    hal_system_shutdown();
}

void cp0_system_reboot(void)
{
    hal_system_reboot();
}

cp0_pty_t cp0_pty_open(const char *cmd, const char *const *args, int cols, int rows)
{
    return reinterpret_cast<cp0_pty_t>(hal_pty_open(cmd, args, cols, rows));
}

int cp0_pty_read(cp0_pty_t pty, char *buf, size_t buf_size)
{
    return hal_pty_read(reinterpret_cast<hal_pty_t>(pty), buf, buf_size);
}

int cp0_pty_write(cp0_pty_t pty, const char *buf, size_t len)
{
    return hal_pty_write(reinterpret_cast<hal_pty_t>(pty), buf, len);
}

int cp0_pty_check_child(cp0_pty_t pty, int *exit_status)
{
    return hal_pty_check_child(reinterpret_cast<hal_pty_t>(pty), exit_status);
}

void cp0_pty_close(cp0_pty_t pty)
{
    hal_pty_close(reinterpret_cast<hal_pty_t>(pty));
}

int cp0_screenshot_save(const char *dir)
{
    return hal_screenshot_save(dir);
}

cp0_battery_info_t cp0_battery_read(void)
{
    hal_battery_info_t hal = hal_battery_read();
    cp0_battery_info_t info{};
    info.voltage_mv = hal.voltage_mv;
    info.current_ma = hal.current_ma;
    info.temperature_c10 = hal.temperature_c10;
    info.soc = hal.soc;
    info.remain_mah = hal.remain_mah;
    info.full_mah = hal.full_mah;
    info.flags = hal.flags;
    info.avg_current_ma = hal.avg_current_ma;
    info.valid = hal.valid;
    return info;
}

int cp0_backlight_read(void)
{
    return hal_backlight_read();
}

int cp0_backlight_max(void)
{
    return hal_backlight_max();
}

int cp0_backlight_write(int val)
{
    return hal_backlight_write(val);
}

int cp0_volume_read(void)
{
    return hal_volume_read();
}

int cp0_volume_write(int val)
{
    return hal_volume_write(val);
}

cp0_wifi_status_t cp0_wifi_get_status(void)
{
    hal_wifi_status_t hal = hal_wifi_get_status();
    cp0_wifi_status_t st{};
    st.connected = hal.connected;
    std::strncpy(st.ssid, hal.ssid, sizeof(st.ssid) - 1);
    std::strncpy(st.ip, hal.ip, sizeof(st.ip) - 1);
    st.signal = hal.signal;
    return st;
}

int cp0_wifi_scan(cp0_wifi_ap_t *out, int max_aps)
{
    if (!out || max_aps <= 0) {
        return hal_wifi_scan(nullptr, 0);
    }

    hal_wifi_ap_t hal_aps[max_aps];
    int count = hal_wifi_scan(hal_aps, max_aps);
    if (count > max_aps) {
        count = max_aps;
    }

    for (int i = 0; i < count; ++i) {
        std::strncpy(out[i].ssid, hal_aps[i].ssid, sizeof(out[i].ssid) - 1);
        out[i].ssid[sizeof(out[i].ssid) - 1] = '\0';
        out[i].signal = hal_aps[i].signal;
        std::strncpy(out[i].security, hal_aps[i].security, sizeof(out[i].security) - 1);
        out[i].security[sizeof(out[i].security) - 1] = '\0';
        out[i].in_use = hal_aps[i].in_use;
    }

    return count;
}

int cp0_wifi_connect(const char *ssid, const char *password)
{
    return hal_wifi_connect(ssid, password);
}

int cp0_wifi_disconnect(void)
{
    return hal_wifi_disconnect();
}

cp0_bt_status_t cp0_bt_get_status(void)
{
    hal_bt_status_t hal = hal_bt_get_status();
    cp0_bt_status_t st{};
    st.powered = hal.powered;
    std::strncpy(st.address, hal.address, sizeof(st.address) - 1);
    return st;
}

int cp0_bt_set_power(int on)
{
    return hal_bt_set_power(on);
}

int cp0_bt_scan(cp0_bt_device_t *out, int max_devices)
{
    if (!out || max_devices <= 0) {
        return hal_bt_scan(nullptr, 0);
    }

    hal_bt_device_t hal_devices[max_devices];
    int count = hal_bt_scan(hal_devices, max_devices);
    if (count > max_devices) {
        count = max_devices;
    }

    for (int i = 0; i < count; ++i) {
        std::strncpy(out[i].name, hal_devices[i].name, sizeof(out[i].name) - 1);
        out[i].name[sizeof(out[i].name) - 1] = '\0';
        std::strncpy(out[i].address, hal_devices[i].address, sizeof(out[i].address) - 1);
        out[i].address[sizeof(out[i].address) - 1] = '\0';
        out[i].rssi = hal_devices[i].rssi;
        out[i].connected = hal_devices[i].connected;
    }

    return count;
}

void cp0_time_str(char *buf, int buf_size)
{
    hal_time_str(buf, buf_size);
}

}
