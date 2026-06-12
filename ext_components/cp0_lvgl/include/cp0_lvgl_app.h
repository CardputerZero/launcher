#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CP0_WIFI_AP_MAX 32
#define CP0_WIFI_SSID_MAX 64
#define CP0_BT_DEVICE_MAX 16
#define CP0_BT_NAME_MAX 64

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
} cp0_battery_info_t;

typedef struct {
    char ssid[CP0_WIFI_SSID_MAX];
    int signal;
    char security[32];
    int in_use;
} cp0_wifi_ap_t;

typedef struct {
    int connected;
    char ssid[CP0_WIFI_SSID_MAX];
    char ip[48];
    int signal;
} cp0_wifi_status_t;

typedef struct {
    int powered;
    char address[24];
} cp0_bt_status_t;

typedef struct {
    char name[CP0_BT_NAME_MAX];
    char address[24];
    int rssi;
    int connected;
} cp0_bt_device_t;

typedef struct {
    char iface[32];
    char ipv4[16];
    char netmask[16];
    int is_up;
} cp0_netif_info_t;

typedef struct {
    char name[256];
    int is_dir;
} cp0_dirent_t;

typedef void *cp0_watcher_t;
typedef void *cp0_pty_t;
typedef int cp0_pid_t;

void cp0_signal_audio_api_play_file(const char *path);
void cp0_signal_audio_api_play_asset(const char *name);
void cp0_signal_system_play_asset(const char *name);

void cp0_config_init(void);
int cp0_config_get_int(const char *key, int default_val);
void cp0_config_set_int(const char *key, int val);
const char *cp0_config_get_str(const char *key, const char *default_val);
void cp0_config_set_str(const char *key, const char *val);
void cp0_config_save(void);


const char *cp0_file_path_c(const char *file);

int cp0_dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count);
cp0_watcher_t cp0_dir_watch_start(const char *path);
int cp0_dir_watch_poll(cp0_watcher_t watcher);
void cp0_dir_watch_stop(cp0_watcher_t watcher);

int cp0_network_list(cp0_netif_info_t *entries, int max_entries, int *out_count);

int cp0_process_exec_blocking(const char *exec_path, volatile int *home_key_flag, int keep_root);
cp0_pid_t cp0_process_spawn(const char *exec_path, int keep_root);
void cp0_process_stop(cp0_pid_t pid);
int cp0_process_check_lock(const char *lock_path, int *holder_pid);
void cp0_process_kill(int pid, int grace_ms);
void cp0_system_shutdown(void);
void cp0_system_reboot(void);

cp0_pty_t cp0_pty_open(const char *cmd, const char *const *args, int cols, int rows);
int cp0_pty_read(cp0_pty_t pty, char *buf, size_t buf_size);
int cp0_pty_write(cp0_pty_t pty, const char *buf, size_t len);
int cp0_pty_check_child(cp0_pty_t pty, int *exit_status);
void cp0_pty_close(cp0_pty_t pty);

int cp0_screenshot_save(const char *dir);

cp0_battery_info_t cp0_battery_read(void);
int cp0_backlight_read(void);
int cp0_backlight_max(void);
int cp0_backlight_write(int val);
int cp0_volume_read(void);
int cp0_volume_write(int val);
cp0_wifi_status_t cp0_wifi_get_status(void);
int cp0_wifi_scan(cp0_wifi_ap_t *out, int max_aps);
int cp0_wifi_connect(const char *ssid, const char *password);
int cp0_wifi_disconnect(void);
cp0_bt_status_t cp0_bt_get_status(void);
int cp0_bt_set_power(int on);
int cp0_bt_scan(cp0_bt_device_t *out, int max_devices);
void cp0_time_str(char *buf, int buf_size);

#ifdef __cplusplus
}
#else
#define cp0_file_path(file) cp0_file_path_c(file)
#endif
