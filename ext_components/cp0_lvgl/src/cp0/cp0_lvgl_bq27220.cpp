#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <cerrno>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <unistd.h>

#if __has_include(<sys/ioctl.h>)
#include <sys/ioctl.h>
#endif

#if __has_include(<linux/i2c.h>) && __has_include(<linux/i2c-dev.h>)
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#define CP0_HAS_LINUX_I2C_RDWR 1
#else
#define CP0_HAS_LINUX_I2C_RDWR 0
#endif

namespace {

class Bq27220System
{
public:
    using arg_t = std::list<std::string>;
    using callback_t = std::function<void(int, std::string)>;

    void api_call(arg_t arg, callback_t callback)
    {
        const std::string cmd = arg.empty() ? "" : arg.front();
        if (cmd == "Read") {
            cp0_battery_info_t info = read();
            report(callback, info.valid ? 0 : -1, encode(info));
        } else if (cmd == "Calibrate") {
            const int index = arg.size() >= 2 ? std::atoi(nth_arg(arg, 1).c_str()) : -1;
            report(callback, calibrate(index), "");
        } else {
            report(callback, -1, "unknown bq27220 api command");
        }
    }

    cp0_battery_info_t read()
    {
        cp0_battery_info_t info{};

        char bq_path[256] = {0};
        long capacity = 0, voltage_uv = 0, current_raw = 0, temp_raw = 0;
        char status[64] = "Unknown";

        if (find_power_supply(bq_path, sizeof(bq_path))) {
            char path[320];
            std::snprintf(path, sizeof(path), "%s/capacity", bq_path);
            int ok = read_long(path, &capacity);
            std::snprintf(path, sizeof(path), "%s/voltage_now", bq_path);
            ok = ok && read_long(path, &voltage_uv);
            std::snprintf(path, sizeof(path), "%s/current_now", bq_path);
            ok = ok && read_long(path, &current_raw);
            std::snprintf(path, sizeof(path), "%s/temp", bq_path);
            ok = ok && read_long(path, &temp_raw);
            std::snprintf(path, sizeof(path), "%s/status", bq_path);
            read_string(path, status, sizeof(status));

            if (ok) {
                const double current_ma = -(current_raw / 1000.0);
                double temp_c = temp_raw / 10.0;
                if (temp_c > 100.0 || temp_c < -40.0) {
                    temp_c = temp_raw / 100.0;
                }

                info.soc = static_cast<int>(capacity);
                info.voltage_mv = static_cast<int>(voltage_uv / 1000);
                info.current_ma = round_to_int(current_ma);
                info.avg_current_ma = info.current_ma;
                info.temperature_c10 = round_to_int(temp_c * 10.0);
                info.flags = (std::strcmp(status, "Charging") == 0) ? 1 : 0;
                info.valid = 1;
                return info;
            }
        }

#if CP0_HAS_LINUX_I2C_RDWR
        int fd = open(kI2cDev, O_RDWR);
        if (fd < 0) {
            return info;
        }

        int v;
        v = read_word(fd, 0x08); if (v >= 0) info.voltage_mv = v;
        v = read_word(fd, 0x0C); if (v >= 0) info.current_ma = (v > 32767) ? v - 65536 : v;
        v = read_word(fd, 0x06); if (v >= 0) info.temperature_c10 = v - 2731;
        v = read_word(fd, 0x2C); if (v >= 0) info.soc = v;
        v = read_word(fd, 0x10); if (v >= 0) info.remain_mah = v;
        v = read_word(fd, 0x12); if (v >= 0) info.full_mah = v;
        v = read_word(fd, 0x0E); if (v >= 0) info.flags = v;
        v = read_word(fd, 0x14); if (v >= 0) info.avg_current_ma = (v > 32767) ? v - 65536 : v;

        info.valid = 1;
        close(fd);
#endif
        return info;
    }

    int calibrate(int command_index)
    {
#if CP0_HAS_LINUX_I2C_RDWR
        static const int cmds[] = {0x0081, 0x000A, 0x0009, 0x0080};
        if (command_index < 0 || command_index >= static_cast<int>(sizeof(cmds) / sizeof(cmds[0]))) {
            return -1;
        }

        int fd = open(kI2cDev, O_RDWR);
        if (fd < 0) {
            return -errno;
        }

        struct i2c_msg msg;
        struct i2c_rdwr_ioctl_data data;
        unsigned char buf[3] = {0x00,
                                static_cast<unsigned char>(cmds[command_index] & 0xFF),
                                static_cast<unsigned char>((cmds[command_index] >> 8) & 0xFF)};
        msg.addr = kI2cAddr;
        msg.flags = 0;
        msg.len = 3;
        msg.buf = buf;
        data.msgs = &msg;
        data.nmsgs = 1;

        int ret = ioctl(fd, I2C_RDWR, &data);
        int saved_errno = errno;
        close(fd);
        return ret == 0 ? 0 : -saved_errno;
#else
        (void)command_index;
        return -1;
#endif
    }

private:
    static constexpr const char *kI2cDev = "/dev/i2c-1";
    static constexpr int kI2cAddr = 0x55;

    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback) {
            callback(code, data);
        }
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        std::advance(it, static_cast<long>(std::min(index, arg.size())));
        return it == arg.end() ? std::string() : *it;
    }

    static int round_to_int(double value)
    {
        return static_cast<int>(value >= 0 ? value + 0.5 : value - 0.5);
    }

    static std::string encode(const cp0_battery_info_t &info)
    {
        std::ostringstream os;
        os << info.voltage_mv << ','
           << info.current_ma << ','
           << info.temperature_c10 << ','
           << info.soc << ','
           << info.remain_mah << ','
           << info.full_mah << ','
           << info.flags << ','
           << info.avg_current_ma << ','
           << info.valid;
        return os.str();
    }

    static bool decode(const std::string &data, cp0_battery_info_t *info)
    {
        if (!info) {
            return false;
        }
        cp0_battery_info_t parsed{};
        char comma;
        std::istringstream is(data);
        if (is >> parsed.voltage_mv >> comma &&
            is >> parsed.current_ma >> comma &&
            is >> parsed.temperature_c10 >> comma &&
            is >> parsed.soc >> comma &&
            is >> parsed.remain_mah >> comma &&
            is >> parsed.full_mah >> comma &&
            is >> parsed.flags >> comma &&
            is >> parsed.avg_current_ma >> comma &&
            is >> parsed.valid) {
            *info = parsed;
            return true;
        }
        return false;
    }

    static int read_long(const char *path, long *value)
    {
        if (!path || !value) return 0;
        FILE *fp = std::fopen(path, "r");
        if (!fp) return 0;
        long v = 0;
        int ret = std::fscanf(fp, "%ld", &v);
        std::fclose(fp);
        if (ret != 1) return 0;
        *value = v;
        return 1;
    }

    static int read_string(const char *path, char *buf, size_t len)
    {
        if (!path || !buf || len == 0) return 0;
        FILE *fp = std::fopen(path, "r");
        if (!fp) return 0;
        if (!std::fgets(buf, static_cast<int>(len), fp)) {
            std::fclose(fp);
            return 0;
        }
        std::fclose(fp);
        size_t n = std::strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
            buf[--n] = 0;
        }
        return 1;
    }

    static int has_file(const char *dir, const char *name)
    {
        char path[320];
        std::snprintf(path, sizeof(path), "%s/%s", dir, name);
        return access(path, R_OK) == 0;
    }

    static int find_power_supply(char *out, size_t out_len)
    {
        const char *base = "/sys/class/power_supply";
        DIR *dp = opendir(base);
        if (!dp) return 0;

        char fallback[320] = {0};
        struct dirent *ent = nullptr;
        while ((ent = readdir(dp)) != nullptr) {
            if (ent->d_name[0] == '.') continue;

            char dir[320];
            std::snprintf(dir, sizeof(dir), "%s/%s", base, ent->d_name);
            if (!has_file(dir, "capacity") ||
                !has_file(dir, "voltage_now") ||
                !has_file(dir, "current_now") ||
                !has_file(dir, "temp") ||
                !has_file(dir, "status")) {
                continue;
            }

            if (std::strstr(ent->d_name, "bq27220") || std::strstr(ent->d_name, "bq27")) {
                std::snprintf(out, out_len, "%s", dir);
                closedir(dp);
                return 1;
            }
            if (fallback[0] == 0) {
                std::snprintf(fallback, sizeof(fallback), "%s", dir);
            }
        }

        closedir(dp);
        if (fallback[0]) {
            std::snprintf(out, out_len, "%s", fallback);
            return 1;
        }
        return 0;
    }

#if CP0_HAS_LINUX_I2C_RDWR
    static int read_word(int fd, unsigned char reg)
    {
        unsigned char buf[2] = {0};
        struct i2c_msg msgs[2];
        struct i2c_rdwr_ioctl_data data;

        msgs[0].addr = kI2cAddr;
        msgs[0].flags = 0;
        msgs[0].len = 1;
        msgs[0].buf = &reg;
        msgs[1].addr = kI2cAddr;
        msgs[1].flags = I2C_M_RD;
        msgs[1].len = 2;
        msgs[1].buf = buf;
        data.msgs = msgs;
        data.nmsgs = 2;

        if (ioctl(fd, I2C_RDWR, &data) < 0) return -1;
        return buf[0] | (buf[1] << 8);
    }
#endif

public:
    static bool decode_info(const std::string &data, cp0_battery_info_t *info)
    {
        return decode(data, info);
    }
};

} // namespace

extern "C" {

cp0_battery_info_t cp0_battery_read(void)
{
    cp0_battery_info_t info{};
    cp0_signal_bq27220_api({"Read"}, [&](int code, std::string data) {
        if (code == 0) {
            Bq27220System::decode_info(data, &info);
        }
    });
    return info;
}

int cp0_bq27220_calibrate(int command_index)
{
    int ret = -1;
    cp0_signal_bq27220_api({"Calibrate", std::to_string(command_index)}, [&](int code, std::string data) {
        (void)data;
        ret = code;
    });
    return ret;
}

void init_bq27220(void)
{
    std::shared_ptr<Bq27220System> bq27220 = std::make_shared<Bq27220System>();
    cp0_signal_bq27220_api.append([bq27220](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        bq27220->api_call(std::move(arg), std::move(callback));
    });
}

}
