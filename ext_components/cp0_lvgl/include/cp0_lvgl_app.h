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

typedef struct {
    char ipv4[48];
    char gateway[48];
    char mac[32];
} cp0_eth_info_t;

typedef struct {
    char user[64];
    char hostname[128];
} cp0_account_info_t;

typedef struct {
    char status[64];
    float yaw;
    float pitch;
    float roll;
    float acc_x;
    float acc_y;
    float acc_z;
    float gyr_x;
    float gyr_y;
    float gyr_z;
    float mag_x;
    float mag_y;
    float mag_z;
    int sensor_ready;
} cp0_compass_info_t;

typedef struct {
    int initialized;
    int hw_ready;
    int tx_mode;
    int tx_in_progress;
    int has_sent_message;
    int rx_event;
    int tx_event;
    char spi_device[64];
    char last_rx[128];
    char last_tx[128];
    char diag[256];
    char probe_summary[256];
    char probe_display[128];
    char pi4io_status[160];
    float rssi;
    float snr;
} cp0_lora_info_t;

typedef void *cp0_watcher_t;
typedef int cp0_pid_t;
typedef void (*cp0_compass_read_cb_t)(int code, const cp0_compass_info_t *info, void *user);

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

int cp0_process_run_argv(const char *const *argv, int background);
int cp0_process_capture_argv(const char *const *argv, char *out, int out_size);
int cp0_file_read_first_line(const char *path, char *out, int out_size);
int cp0_desktop_exec_is_safe(const char *exec, char *reason, int reason_size);
int cp0_network_default_info_read(cp0_eth_info_t *info);
int cp0_eth_info_read(cp0_eth_info_t *info);
int cp0_account_info_read(cp0_account_info_t *info);
int cp0_system_apt_update_background(void);
int cp0_system_update_launcher_background(void);
int cp0_time_set(const char *timestamp);
int cp0_bq27220_calibrate(int command_index);
int cp0_compass_read(cp0_compass_read_cb_t callback, void *user);
int cp0_compass_calibrate(void);
cp0_battery_info_t cp0_battery_read(void);
int cp0_backlight_read(void);
int cp0_backlight_max(void);
int cp0_backlight_write(int val);
cp0_bt_status_t cp0_bt_get_status(void);
int cp0_bt_set_power(int on);
int cp0_bt_scan(cp0_bt_device_t *out, int max_devices);
void cp0_time_str(char *buf, int buf_size);

#ifdef __cplusplus
}
#else
#define cp0_file_path(file) cp0_file_path_c(file)
#endif
