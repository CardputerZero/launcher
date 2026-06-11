#include "cp0_lvgl_app.h"
#include "hal/hal_audio.h"
#include "hal/hal_config.h"
#include "hal/hal_filesystem.h"
#include "hal/hal_network.h"
#include "hal/hal_paths.h"
#include "hal/hal_process.h"
#include "hal/hal_pty.h"
#include "hal/hal_screenshot.h"
#include "hal/hal_settings.h"
#include "hal_lvgl_bsp.h"

#include <cstring>
#include <list>
#include <string>

static_assert(sizeof(cp0_battery_info_t) == sizeof(hal_battery_info_t), "battery ABI mismatch");
static_assert(sizeof(cp0_wifi_ap_t) == sizeof(hal_wifi_ap_t), "wifi AP ABI mismatch");
static_assert(sizeof(cp0_wifi_status_t) == sizeof(hal_wifi_status_t), "wifi status ABI mismatch");
static_assert(sizeof(cp0_bt_status_t) == sizeof(hal_bt_status_t), "bt status ABI mismatch");
static_assert(sizeof(cp0_bt_device_t) == sizeof(hal_bt_device_t), "bt device ABI mismatch");
static_assert(sizeof(cp0_netif_info_t) == sizeof(hal_netif_info_t), "network ABI mismatch");
static_assert(sizeof(cp0_dirent_t) == sizeof(hal_dirent_t), "dirent ABI mismatch");

extern "C" {

void cp0_audio_init(void) { hal_audio_init(); }
void cp0_audio_play(const char *path)
{
    if (path && path[0])
        cp0_signal_audio_play(std::string(path));
}
void cp0_audio_play_sync(const char *path) { hal_audio_play_sync(path); }
void cp0_audio_stop(void) { hal_audio_stop(); }
void cp0_audio_deinit(void) { hal_audio_deinit(); }

void cp0_config_init(void) { hal_config_init(); }
int cp0_config_get_int(const char *key, int default_val) { return hal_config_get_int(key, default_val); }
void cp0_config_set_int(const char *key, int val) { hal_config_set_int(key, val); }
const char *cp0_config_get_str(const char *key, const char *default_val) { return hal_config_get_str(key, default_val); }
void cp0_config_set_str(const char *key, const char *val) { hal_config_set_str(key, val); }
void cp0_config_save(void) { hal_config_save(); }

void cp0_paths_init(const char *exe_dir) { hal_paths_init(exe_dir); }
const char *cp0_path_data_dir(void) { return hal_path_data_dir(); }
const char *cp0_path_applications_dir(void) { return hal_path_applications_dir(); }
const char *cp0_path_store_cache_dir(void) { return hal_path_store_cache_dir(); }
const char *cp0_path_lock_file(void) { return hal_path_lock_file(); }
const char *cp0_path_font_dir(void) { return hal_path_font_dir(); }
const char *cp0_path_font_regular(void) { return hal_path_font_regular(); }
const char *cp0_path_font_mono(void) { return hal_path_font_mono(); }
const char *cp0_path_keyboard_device(void) { return hal_path_keyboard_device(); }
const char *cp0_path_keyboard_map(void) { return hal_path_keyboard_map(); }
const char *cp0_path_store_sync_cmd(void) { return hal_path_store_sync_cmd(); }
const char *cp0_path_images_dir(void) { return hal_path_images_dir(); }
const char *cp0_path_audio_dir(void) { return hal_path_audio_dir(); }

int cp0_dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count)
{
    return hal_dir_list(path, reinterpret_cast<hal_dirent_t *>(entries), max_entries, out_count);
}
cp0_watcher_t cp0_dir_watch_start(const char *path) { return hal_dir_watch_start(path); }
int cp0_dir_watch_poll(cp0_watcher_t watcher) { return hal_dir_watch_poll(reinterpret_cast<hal_watcher_t>(watcher)); }
void cp0_dir_watch_stop(cp0_watcher_t watcher) { hal_dir_watch_stop(reinterpret_cast<hal_watcher_t>(watcher)); }

int cp0_network_list(cp0_netif_info_t *entries, int max_entries, int *out_count)
{
    return hal_network_list(reinterpret_cast<hal_netif_info_t *>(entries), max_entries, out_count);
}

int cp0_process_exec_blocking(const char *exec_path, volatile int *home_key_flag, int keep_root)
{
    return hal_process_exec_blocking(exec_path, home_key_flag, keep_root);
}
cp0_pid_t cp0_process_spawn(const char *exec_path, int keep_root) { return hal_process_spawn(exec_path, keep_root); }
void cp0_process_stop(cp0_pid_t pid) { hal_process_stop(pid); }
int cp0_process_check_lock(const char *lock_path, int *holder_pid) { return hal_process_check_lock(lock_path, holder_pid); }
void cp0_process_kill(int pid, int grace_ms) { hal_process_kill(pid, grace_ms); }
void cp0_system_shutdown(void) { hal_system_shutdown(); }
void cp0_system_reboot(void) { hal_system_reboot(); }

cp0_pty_t cp0_pty_open(const char *cmd, const char *const *args, int cols, int rows)
{
    return hal_pty_open(cmd, args, cols, rows);
}
int cp0_pty_read(cp0_pty_t pty, char *buf, size_t buf_size) { return hal_pty_read(reinterpret_cast<hal_pty_t>(pty), buf, buf_size); }
int cp0_pty_write(cp0_pty_t pty, const char *buf, size_t len) { return hal_pty_write(reinterpret_cast<hal_pty_t>(pty), buf, len); }
int cp0_pty_check_child(cp0_pty_t pty, int *exit_status) { return hal_pty_check_child(reinterpret_cast<hal_pty_t>(pty), exit_status); }
void cp0_pty_close(cp0_pty_t pty) { hal_pty_close(reinterpret_cast<hal_pty_t>(pty)); }

int cp0_screenshot_save(const char *dir) { return hal_screenshot_save(dir); }

cp0_battery_info_t cp0_battery_read(void)
{
    hal_battery_info_t hal = hal_battery_read();
    cp0_battery_info_t out;
    std::memcpy(&out, &hal, sizeof(out));
    return out;
}
int cp0_backlight_read(void) { return hal_backlight_read(); }
int cp0_backlight_max(void) { return hal_backlight_max(); }
int cp0_backlight_write(int val) { return hal_backlight_write(val); }
int cp0_volume_read(void) { return hal_volume_read(); }
int cp0_volume_write(int val) { return hal_volume_write(val); }
cp0_wifi_status_t cp0_wifi_get_status(void)
{
    hal_wifi_status_t hal = hal_wifi_get_status();
    cp0_wifi_status_t out;
    std::memcpy(&out, &hal, sizeof(out));
    return out;
}
int cp0_wifi_scan(cp0_wifi_ap_t *out, int max_aps)
{
    return hal_wifi_scan(reinterpret_cast<hal_wifi_ap_t *>(out), max_aps);
}
int cp0_wifi_connect(const char *ssid, const char *password) { return hal_wifi_connect(ssid, password); }
int cp0_wifi_disconnect(void) { return hal_wifi_disconnect(); }
cp0_bt_status_t cp0_bt_get_status(void)
{
    hal_bt_status_t hal = hal_bt_get_status();
    cp0_bt_status_t out;
    std::memcpy(&out, &hal, sizeof(out));
    return out;
}
int cp0_bt_set_power(int on) { return hal_bt_set_power(on); }
int cp0_bt_scan(cp0_bt_device_t *out, int max_devices)
{
    return hal_bt_scan(reinterpret_cast<hal_bt_device_t *>(out), max_devices);
}
void cp0_time_str(char *buf, int buf_size) { hal_time_str(buf, buf_size); }

}
