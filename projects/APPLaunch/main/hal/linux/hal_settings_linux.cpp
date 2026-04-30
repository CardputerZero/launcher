#include "../hal_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <dirent.h>

static int bq27220_read_word(int fd, unsigned char reg)
{
    unsigned char buf[2] = {0};
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data data;

    msgs[0].addr  = 0x55;
    msgs[0].flags = 0;
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;
    msgs[1].addr  = 0x55;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len   = 2;
    msgs[1].buf   = buf;
    data.msgs  = msgs;
    data.nmsgs = 2;

    if (ioctl(fd, I2C_RDWR, &data) < 0) return -1;
    return buf[0] | (buf[1] << 8);
}

static int bqmon_read_long(const char *path, long *value)
{
    if (!path || !value) return 0;
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    long v = 0;
    int ret = fscanf(fp, "%ld", &v);
    fclose(fp);
    if (ret != 1) return 0;
    *value = v;
    return 1;
}

static int bqmon_read_string(const char *path, char *buf, size_t len)
{
    if (!path || !buf || len == 0) return 0;
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    if (!fgets(buf, len, fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = 0;
    return 1;
}

static double bqmon_current_ma(long current_now)
{
    return -(current_now / 1000.0);
}

static double bqmon_temp_c(long temp)
{
    double c = temp / 10.0;
    if (c > 100.0 || c < -40.0)
        c = temp / 100.0;
    return c;
}

static int bqmon_has_file(const char *dir, const char *name)
{
    char path[320];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    return access(path, R_OK) == 0;
}

static int bqmon_find_power_supply(char *out, size_t out_len)
{
    const char *base = "/sys/class/power_supply";
    DIR *dp = opendir(base);
    if (!dp) return 0;

    char fallback[320] = {0};
    struct dirent *ent = NULL;
    while ((ent = readdir(dp)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char dir[320];
        snprintf(dir, sizeof(dir), "%s/%s", base, ent->d_name);
        if (!bqmon_has_file(dir, "capacity") ||
            !bqmon_has_file(dir, "voltage_now") ||
            !bqmon_has_file(dir, "current_now") ||
            !bqmon_has_file(dir, "temp") ||
            !bqmon_has_file(dir, "status"))
            continue;

        if (strstr(ent->d_name, "bq27220") || strstr(ent->d_name, "bq27")) {
            snprintf(out, out_len, "%s", dir);
            closedir(dp);
            return 1;
        }
        if (fallback[0] == 0)
            snprintf(fallback, sizeof(fallback), "%s", dir);
    }

    closedir(dp);
    if (fallback[0]) {
        snprintf(out, out_len, "%s", fallback);
        return 1;
    }
    return 0;
}

hal_battery_info_t hal_battery_read(void)
{
    hal_battery_info_t info;
    memset(&info, 0, sizeof(info));

    char bq_path[256] = {0};
    long capacity = 0, voltage_uv = 0, current_raw = 0, temp_raw = 0;
    char status[64] = "Unknown";

    if (bqmon_find_power_supply(bq_path, sizeof(bq_path))) {
        char path[320];
        snprintf(path, sizeof(path), "%s/capacity", bq_path);
        int ok = bqmon_read_long(path, &capacity);
        snprintf(path, sizeof(path), "%s/voltage_now", bq_path);
        ok = ok && bqmon_read_long(path, &voltage_uv);
        snprintf(path, sizeof(path), "%s/current_now", bq_path);
        ok = ok && bqmon_read_long(path, &current_raw);
        snprintf(path, sizeof(path), "%s/temp", bq_path);
        ok = ok && bqmon_read_long(path, &temp_raw);
        snprintf(path, sizeof(path), "%s/status", bq_path);
        bqmon_read_string(path, status, sizeof(status));

        if (ok) {
            double current_ma = bqmon_current_ma(current_raw);
            double temp_c = bqmon_temp_c(temp_raw);

            info.soc = (int)capacity;
            info.voltage_mv = (int)(voltage_uv / 1000);
            info.current_ma = (int)(current_ma >= 0 ? current_ma + 0.5 : current_ma - 0.5);
            info.avg_current_ma = info.current_ma;
            info.temperature_c10 = (int)(temp_c >= 0 ? temp_c * 10.0 + 0.5 : temp_c * 10.0 - 0.5);
            info.flags = (strcmp(status, "Charging") == 0) ? 1 : 0;
            info.valid = 1;
            return info;
        }
    }

    int fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) return info;

    int v;
    v = bq27220_read_word(fd, 0x08); if (v >= 0) info.voltage_mv = v;
    v = bq27220_read_word(fd, 0x0C); if (v >= 0) info.current_ma = (v > 32767) ? v - 65536 : v;
    v = bq27220_read_word(fd, 0x06); if (v >= 0) info.temperature_c10 = v - 2731;
    v = bq27220_read_word(fd, 0x2C); if (v >= 0) info.soc = v;
    v = bq27220_read_word(fd, 0x10); if (v >= 0) info.remain_mah = v;
    v = bq27220_read_word(fd, 0x12); if (v >= 0) info.full_mah = v;
    v = bq27220_read_word(fd, 0x0E); if (v >= 0) info.flags = v;
    v = bq27220_read_word(fd, 0x14); if (v >= 0) info.avg_current_ma = (v > 32767) ? v - 65536 : v;

    info.valid = 1;
    close(fd);
    return info;
}

int hal_backlight_read(void)
{
    FILE *f = fopen("/sys/class/backlight/backlight/brightness", "r");
    if (!f) return -1;
    int val = 0;
    if (fscanf(f, "%d", &val) != 1) val = -1;
    fclose(f);
    return val;
}

int hal_backlight_max(void)
{
    FILE *f = fopen("/sys/class/backlight/backlight/max_brightness", "r");
    if (!f) return 100;
    int val = 100;
    if (fscanf(f, "%d", &val) != 1) val = 100;
    fclose(f);
    return val;
}

int hal_backlight_write(int val)
{
    if (val < 0) val = 0;
    int mx = hal_backlight_max();
    if (val > mx) val = mx;
    FILE *f = fopen("/sys/class/backlight/backlight/brightness", "w");
    if (!f) return -1;
    fprintf(f, "%d", val);
    fclose(f);
    return val;
}

int hal_volume_read(void)
{
    FILE *p = popen("amixer -c1 sget 'Headphone Playback Volume' 2>/dev/null", "r");
    if (!p) return -1;
    char buf[256];
    int val = -1;
    while (fgets(buf, sizeof(buf), p)) {
        char *s = strstr(buf, ": values=");
        if (s) { val = atoi(s + 9); break; }
    }
    pclose(p);
    return val;
}

int hal_volume_write(int val)
{
    if (val < 0) val = 0;
    if (val > 63) val = 63;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "amixer -c1 sset 'Headphone Playback Volume' %d 2>/dev/null", val);
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    char buf[128];
    while (fgets(buf, sizeof(buf), p)) {}
    pclose(p);
    return val;
}

hal_wifi_status_t hal_wifi_get_status(void)
{
    hal_wifi_status_t st;
    memset(&st, 0, sizeof(st));
    FILE *p = popen("nmcli -t -f active,ssid,signal dev wifi list 2>/dev/null", "r");
    if (!p) return st;
    char line[256];
    while (fgets(line, sizeof(line), p)) {
        line[strcspn(line, "\n")] = 0;
        if (strncmp(line, "yes:", 4) == 0) {
            st.connected = 1;
            char *s = line + 4;
            char *colon = strrchr(s, ':');
            if (colon) {
                *colon = 0;
                strncpy(st.ssid, s, WIFI_SSID_MAX - 1);
                st.signal = atoi(colon + 1);
            }
            break;
        }
    }
    pclose(p);
    if (st.connected) {
        p = popen("nmcli -t -f IP4.ADDRESS con show --active 2>/dev/null", "r");
        if (p) {
            while (fgets(line, sizeof(line), p)) {
                line[strcspn(line, "\n")] = 0;
                char *v = strstr(line, "IP4.ADDRESS");
                if (v) {
                    v = strchr(v, ':');
                    if (v) { v++; char *sl = strchr(v, '/'); if (sl) *sl = 0; strncpy(st.ip, v, sizeof(st.ip) - 1); }
                    break;
                }
            }
            pclose(p);
        }
    }
    return st;
}

int hal_wifi_scan(hal_wifi_ap_t *out, int max_aps)
{
    system("nmcli dev wifi rescan 2>/dev/null");
    usleep(500000);
    FILE *p = popen("nmcli -t -f SSID,SIGNAL,SECURITY,IN-USE dev wifi list 2>/dev/null", "r");
    if (!p) return 0;
    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), p) && count < max_aps) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == 0) continue;
        hal_wifi_ap_t *ap = &out[count];
        memset(ap, 0, sizeof(*ap));
        char *ptr = line;
        char *last_colon = strrchr(ptr, ':');
        if (!last_colon) continue;
        ap->in_use = (*(last_colon + 1) == '*') ? 1 : 0;
        *last_colon = 0;
        char *sec_colon = strrchr(ptr, ':');
        if (!sec_colon) continue;
        strncpy(ap->security, sec_colon + 1, sizeof(ap->security) - 1);
        *sec_colon = 0;
        char *sig_colon = strrchr(ptr, ':');
        if (!sig_colon) continue;
        ap->signal = atoi(sig_colon + 1);
        *sig_colon = 0;
        if (ptr[0] == 0) continue;
        strncpy(ap->ssid, ptr, WIFI_SSID_MAX - 1);
        count++;
    }
    pclose(p);
    return count;
}

int hal_wifi_connect(const char *ssid, const char *password)
{
    char cmd[512];
    if (password && password[0])
        snprintf(cmd, sizeof(cmd), "nmcli dev wifi connect '%s' password '%s' 2>&1", ssid, password);
    else
        snprintf(cmd, sizeof(cmd), "nmcli con up id '%s' 2>&1", ssid);
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    char buf[256]; int ok = 0;
    while (fgets(buf, sizeof(buf), p)) { if (strstr(buf, "successfully")) ok = 1; }
    pclose(p);
    return ok ? 0 : -1;
}

hal_bt_status_t hal_bt_get_status(void)
{
    hal_bt_status_t st;
    memset(&st, 0, sizeof(st));
    FILE *p = popen("bluetoothctl show 2>/dev/null", "r");
    if (!p) return st;
    char line[256];
    while (fgets(line, sizeof(line), p)) {
        if (strstr(line, "Powered:")) st.powered = strstr(line, "yes") ? 1 : 0;
        char *addr = strstr(line, "Controller ");
        if (addr) { addr += 11; char *sp = strchr(addr, ' '); if (sp) *sp = 0; addr[strcspn(addr, "\n")] = 0; strncpy(st.address, addr, sizeof(st.address) - 1); }
    }
    pclose(p);
    return st;
}

int hal_bt_set_power(int on)
{
    FILE *p = popen(on ? "bluetoothctl power on 2>/dev/null" : "bluetoothctl power off 2>/dev/null", "r");
    if (!p) return -1;
    char buf[128]; int ok = 0;
    while (fgets(buf, sizeof(buf), p)) { if (strstr(buf, "succeeded") || strstr(buf, "Changing")) ok = 1; }
    pclose(p);
    return ok ? 0 : -1;
}

void hal_time_str(char *buf, int buf_size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, buf_size, "%02d:%02d", t->tm_hour, t->tm_min);
}
