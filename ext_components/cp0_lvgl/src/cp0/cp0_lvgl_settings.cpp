#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"

#ifdef HAL_PLATFORM_SDL
#include "hal/hal_settings.h"
#endif

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <linux/gpio.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

struct ExtPortGpioMap {
    const char *chip_path;
    unsigned int grove5v_line;
    unsigned int ext5v_line;
    bool grove5v_active_low;
    bool ext5v_active_low;
};

static constexpr const char *kHwIdPath = "/proc/cardputerzero_hw_id";
static constexpr ExtPortGpioMap kCardputerZeroGpioMap = {"/dev/gpiochip1", 3, 12, false, false};
static constexpr ExtPortGpioMap kFallbackGpioMap = {"/dev/gpiochip0", 17, 5, false, true};

static int gpio_v2_get_value(int line_fd);

static int gpio_v2_request_output(const char *chip_path, unsigned int line, const char *consumer, int value)
{
#if defined(GPIO_V2_GET_LINE_IOCTL) && defined(GPIO_V2_LINE_SET_VALUES_IOCTL)
    int chip_fd = open(chip_path, O_RDONLY | O_CLOEXEC);
    if (chip_fd < 0)
        return -errno;

    struct gpio_v2_line_request req;
    std::memset(&req, 0, sizeof(req));
    req.offsets[0] = line;
    req.num_lines = 1;
    std::snprintf(req.consumer, sizeof(req.consumer), "%s", consumer ? consumer : "applaunch");
    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;

    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        int err = errno;
        close(chip_fd);
        return -err;
    }
    close(chip_fd);

    struct gpio_v2_line_values vals;
    std::memset(&vals, 0, sizeof(vals));
    vals.mask = 1;
    vals.bits = value ? 1 : 0;
    if (ioctl(req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals) < 0) {
        int err = errno;
        close(req.fd);
        return -err;
    }
    return req.fd;
#else
    (void)chip_path;
    (void)line;
    (void)consumer;
    (void)value;
    return -ENOSYS;
#endif
}

static int gpio_v2_read_input(const char *chip_path, unsigned int line, const char *consumer)
{
#if defined(GPIO_V2_GET_LINE_IOCTL) && defined(GPIO_V2_LINE_GET_VALUES_IOCTL)
    int chip_fd = open(chip_path, O_RDONLY | O_CLOEXEC);
    if (chip_fd < 0)
        return -errno;

    struct gpio_v2_line_request req;
    std::memset(&req, 0, sizeof(req));
    req.offsets[0] = line;
    req.num_lines = 1;
    std::snprintf(req.consumer, sizeof(req.consumer), "%s", consumer ? consumer : "applaunch");
    req.config.flags = GPIO_V2_LINE_FLAG_INPUT;

    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        int err = errno;
        close(chip_fd);
        return -err;
    }
    close(chip_fd);

    int value = gpio_v2_get_value(req.fd);
    close(req.fd);
    return value;
#else
    (void)chip_path;
    (void)line;
    (void)consumer;
    return -ENOSYS;
#endif
}

static int gpio_v2_get_value(int line_fd)
{
#if defined(GPIO_V2_LINE_GET_VALUES_IOCTL)
    if (line_fd < 0)
        return -EBADF;
    struct gpio_v2_line_values vals;
    std::memset(&vals, 0, sizeof(vals));
    vals.mask = 1;
    if (ioctl(line_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &vals) < 0)
        return -errno;
    return (vals.bits & 1) ? 1 : 0;
#else
    (void)line_fd;
    return -ENOSYS;
#endif
}

class SettingsSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    SettingsSystem()
    {
        apply_extport_config();
    }

    void api_call(arg_t arg, callback_t callback)
    {
        const std::string cmd = arg.empty() ? "" : arg.front();
        if (cmd == "BacklightRead") {
            int val = backlight_read();
            report(callback, val < 0 ? -1 : 0, std::to_string(val));
        } else if (cmd == "BacklightMax") {
            int val = backlight_max();
            report(callback, val < 0 ? -1 : 0, std::to_string(val));
        } else if (cmd == "BacklightWrite") {
            int val = backlight_write(std::atoi(nth_arg(arg, 1).c_str()));
            report(callback, val < 0 ? -1 : 0, std::to_string(val));
        } else if (cmd == "BtStatus") {
            report(callback, 0, encode_bt_status(bt_get_status()));
        } else if (cmd == "BtPower") {
            report(callback, bt_set_power(std::atoi(nth_arg(arg, 1).c_str())), "");
        } else if (cmd == "BtScan") {
            int max_count = arg.size() >= 2 ? std::atoi(nth_arg(arg, 1).c_str()) : CP0_BT_DEVICE_MAX;
            std::vector<cp0_bt_device_t> devices(std::max(0, max_count));
            int count = bt_scan(devices.empty() ? nullptr : devices.data(), static_cast<int>(devices.size()));
            report(callback, count, encode_bt_scan(devices.data(), count));
        } else if (cmd == "TimeStr") {
            char buf[32] = {};
            time_str(buf, sizeof(buf));
            report(callback, 0, buf);
        } else if (cmd == "GpioSet") {
            const std::string name = nth_arg(arg, 1);
            int val = std::atoi(nth_arg(arg, 2).c_str());
            int ret = set_named_gpio(name.c_str(), val);
            report(callback, ret, ret == 0 ? "ok" : std::string("gpio set failed: ") + std::to_string(-ret));
        } else if (cmd == "GpioGet") {
            const std::string name = nth_arg(arg, 1);
            int val = get_named_gpio(name.c_str());
            if (val < 0)
                report(callback, val, std::string("gpio get failed: ") + std::to_string(-val));
            else
                report(callback, 0, std::to_string(val));
        } else {
            report(callback, -1, "unknown settings api command");
        }
    }

    static int api_int(const arg_t &arg, int default_value = -1)
    {
        int result = default_value;
        cp0_signal_settings_api(arg, [&](int code, std::string data) {
            if (code >= 0)
                result = std::atoi(data.c_str());
        });
        return result;
    }

    static cp0_bt_status_t api_bt_status()
    {
        cp0_bt_status_t st{};
        cp0_signal_settings_api({"BtStatus"}, [&](int code, std::string data) {
            if (code == 0)
                decode_bt_status(data, st);
        });
        return st;
    }

    static int api_bt_power(int on)
    {
        int result = -1;
        cp0_signal_settings_api({"BtPower", std::to_string(on)}, [&](int code, std::string) {
            result = code;
        });
        return result;
    }

    static int api_bt_scan(cp0_bt_device_t *out, int max_devices)
    {
        int count = 0;
        cp0_signal_settings_api({"BtScan", std::to_string(max_devices)}, [&](int code, std::string data) {
            count = out && max_devices > 0 ? decode_bt_scan(data, out, max_devices) : code;
        });
        return count;
    }

    static void api_time_str(char *buf, int buf_size)
    {
        if (!buf || buf_size <= 0)
            return;
        buf[0] = '\0';
        cp0_signal_settings_api({"TimeStr"}, [&](int code, std::string data) {
            if (code == 0)
                copy_string(buf, static_cast<size_t>(buf_size), data);
        });
        if (buf[0] == '\0')
            fallback_time_str(buf, buf_size);
    }

private:
    std::mutex gpio_mutex_;
    const ExtPortGpioMap *gpio_map_ = nullptr;

    void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    void apply_extport_config()
    {
        set_named_gpio("GROVE5V", config_get_int("extport_usb", 1));
        set_named_gpio("EXT5V", config_get_int("extport_5vout", 1));
    }

    int set_named_gpio(const char *name, int val)
    {
        std::lock_guard<std::mutex> lock(gpio_mutex_);
        const ExtPortGpioMap &map = active_gpio_map_locked();
        if (is_grove5v_name(name))
            return set_gpio_value(map.chip_path, map.grove5v_line, map.grove5v_active_low, "GROVE5V", val);
        if (is_ext5v_name(name))
            return set_gpio_value(map.chip_path, map.ext5v_line, map.ext5v_active_low, "EXT5V", val);
        return -EINVAL;
    }

    int get_named_gpio(const char *name)
    {
        std::lock_guard<std::mutex> lock(gpio_mutex_);
        const ExtPortGpioMap &map = active_gpio_map_locked();
        if (is_grove5v_name(name))
            return get_gpio_value(map.chip_path, map.grove5v_line, map.grove5v_active_low, "GROVE5V");
        if (is_ext5v_name(name))
            return get_gpio_value(map.chip_path, map.ext5v_line, map.ext5v_active_low, "EXT5V");
        return -EINVAL;
    }

    const ExtPortGpioMap &active_gpio_map_locked()
    {
        const ExtPortGpioMap *map = access(kHwIdPath, F_OK) == 0 ? &kCardputerZeroGpioMap : &kFallbackGpioMap;
        if (gpio_map_ != map)
            gpio_map_ = map;
        return *gpio_map_;
    }

    int set_gpio_value(const char *chip_path, unsigned int line, bool active_low, const char *consumer, int val)
    {
        int bit = (val ? 1 : 0) ^ (active_low ? 1 : 0);
        int fd = gpio_v2_request_output(chip_path, line, consumer, bit);
        if (fd < 0)
            return fd;
        close(fd);
        return fd >= 0 ? 0 : fd;
    }

    int get_gpio_value(const char *chip_path, unsigned int line, bool active_low, const char *consumer)
    {
        int value = gpio_v2_read_input(chip_path, line, consumer);
        return value < 0 ? value : (value ^ (active_low ? 1 : 0));
    }

    static int config_get_int(const char *key, int default_val)
    {
        int val = default_val;
        cp0_signal_config_api({"GetInt", key ? std::string(key) : std::string(), std::to_string(default_val)},
                              [&](int code, std::string data) {
                                  if (code == 0)
                                      val = std::atoi(data.c_str());
                              });
        return val;
    }

    static bool is_grove5v_name(const char *name)
    {
        return name && (std::strcmp(name, "GROVE5V") == 0 || std::strcmp(name, "extport_usb") == 0);
    }

    static bool is_ext5v_name(const char *name)
    {
        return name && (std::strcmp(name, "EXT5V") == 0 || std::strcmp(name, "extport_5vout") == 0);
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        std::advance(it, std::min(index, arg.size()));
        return it == arg.end() ? std::string() : *it;
    }

    static void copy_string(char *dst, size_t dst_size, const std::string &src)
    {
        if (!dst || dst_size == 0)
            return;
        std::strncpy(dst, src.c_str(), dst_size - 1);
        dst[dst_size - 1] = '\0';
    }

    static std::vector<std::string> split_char(const std::string &line, char delimiter)
    {
        std::vector<std::string> cols;
        std::string item;
        std::istringstream iss(line);
        while (std::getline(iss, item, delimiter))
            cols.push_back(item);
        return cols;
    }

    static std::string encode_bt_status(const cp0_bt_status_t &st)
    {
        std::ostringstream oss;
        oss << st.powered << '\t' << st.address;
        return oss.str();
    }

    static bool decode_bt_status(const std::string &data, cp0_bt_status_t &st)
    {
        auto cols = split_char(data, '\t');
        if (cols.size() < 2)
            return false;
        st = {};
        st.powered = std::atoi(cols[0].c_str());
        copy_string(st.address, sizeof(st.address), cols[1]);
        return true;
    }

    static std::string encode_bt_scan(const cp0_bt_device_t *devices, int count)
    {
        std::ostringstream oss;
        for (int i = 0; devices && i < count; ++i) {
            oss << devices[i].address << '\t' << devices[i].rssi << '\t' << devices[i].connected << '\t' << devices[i].name << '\n';
        }
        return oss.str();
    }

    static int decode_bt_scan(const std::string &data, cp0_bt_device_t *out, int max_devices)
    {
        if (!out || max_devices <= 0)
            return 0;
        int count = 0;
        std::istringstream lines(data);
        std::string line;
        while (count < max_devices && std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto cols = split_char(line, '\t');
            if (cols.size() < 4 || cols[0].empty())
                continue;
            cp0_bt_device_t dev{};
            copy_string(dev.address, sizeof(dev.address), cols[0]);
            dev.rssi = std::atoi(cols[1].c_str());
            dev.connected = std::atoi(cols[2].c_str());
            copy_string(dev.name, sizeof(dev.name), cols[3]);
            out[count++] = dev;
        }
        return count;
    }

    static void fallback_time_str(char *buf, int buf_size)
    {
        if (!buf || buf_size <= 0)
            return;
        std::time_t now = std::time(nullptr);
        std::tm *t = std::localtime(&now);
        if (!t) {
            buf[0] = '\0';
            return;
        }
        std::snprintf(buf, static_cast<size_t>(buf_size), "%02d:%02d", t->tm_hour, t->tm_min);
    }

#ifdef HAL_PLATFORM_SDL
    int backlight_read() { return hal_backlight_read(); }
    int backlight_max() { return hal_backlight_max(); }
    int backlight_write(int val) { return hal_backlight_write(val); }

    cp0_bt_status_t bt_get_status()
    {
        hal_bt_status_t hal = hal_bt_get_status();
        cp0_bt_status_t st{};
        st.powered = hal.powered;
        copy_string(st.address, sizeof(st.address), hal.address);
        return st;
    }

    int bt_set_power(int on) { return hal_bt_set_power(on); }

    int bt_scan(cp0_bt_device_t *out, int max_devices)
    {
        if (!out || max_devices <= 0)
            return hal_bt_scan(nullptr, 0);

        std::vector<hal_bt_device_t> hal_devices(static_cast<size_t>(max_devices));
        int count = hal_bt_scan(hal_devices.data(), max_devices);
        count = std::min(count, max_devices);
        for (int i = 0; i < count; ++i) {
            copy_string(out[i].name, sizeof(out[i].name), hal_devices[static_cast<size_t>(i)].name);
            copy_string(out[i].address, sizeof(out[i].address), hal_devices[static_cast<size_t>(i)].address);
            out[i].rssi = hal_devices[static_cast<size_t>(i)].rssi;
            out[i].connected = hal_devices[static_cast<size_t>(i)].connected;
        }
        return count;
    }

    void time_str(char *buf, int buf_size) { hal_time_str(buf, buf_size); }
#else
    static int read_int_file(const char *path, int default_value)
    {
        FILE *f = std::fopen(path, "r");
        if (!f)
            return default_value;
        int val = default_value;
        if (std::fscanf(f, "%d", &val) != 1)
            val = default_value;
        std::fclose(f);
        return val;
    }

    int backlight_read()
    {
        return read_int_file("/sys/class/backlight/backlight/brightness", -1);
    }

    int backlight_max()
    {
        return read_int_file("/sys/class/backlight/backlight/max_brightness", 100);
    }

    int backlight_write(int val)
    {
        if (val < 0)
            val = 0;
        int mx = backlight_max();
        if (val > mx)
            val = mx;
        FILE *f = std::fopen("/sys/class/backlight/backlight/brightness", "w");
        if (!f)
            return -1;
        std::fprintf(f, "%d", val);
        std::fclose(f);
        return val;
    }

    cp0_bt_status_t bt_get_status()
    {
        cp0_bt_status_t st{};
        char output[4096] = {};
        const char *argv[] = {"bluetoothctl", "show", nullptr};
        if (cp0_process_capture_argv(argv, output, sizeof(output)) != 0)
            return st;

        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.find("Powered:") != std::string::npos)
                st.powered = line.find("yes") != std::string::npos ? 1 : 0;
            std::string marker = "Controller ";
            size_t pos = line.find(marker);
            if (pos != std::string::npos) {
                std::string addr = line.substr(pos + marker.size());
                size_t sp = addr.find(' ');
                if (sp != std::string::npos)
                    addr.resize(sp);
                copy_string(st.address, sizeof(st.address), addr);
            }
        }
        return st;
    }

    int bt_set_power(int on)
    {
        const char *argv_on[] = {"bluetoothctl", "power", "on", nullptr};
        const char *argv_off[] = {"bluetoothctl", "power", "off", nullptr};
        char output[1024] = {};
        int ret = cp0_process_capture_argv(on ? argv_on : argv_off, output, sizeof(output));
        if (ret != 0)
            return -1;
        std::string data(output);
        return (data.find("succeeded") != std::string::npos || data.find("Changing") != std::string::npos) ? 0 : -1;
    }

    int bt_scan(cp0_bt_device_t *out, int max_devices)
    {
        const char *scan_on[] = {"bluetoothctl", "scan", "on", nullptr};
        const char *scan_off[] = {"bluetoothctl", "scan", "off", nullptr};
        cp0_process_run_argv(scan_on, 1);
        std::this_thread::sleep_for(std::chrono::seconds(4));
        cp0_process_run_argv(scan_off, 0);

        char output[8192] = {};
        const char *devices[] = {"bluetoothctl", "devices", nullptr};
        if (cp0_process_capture_argv(devices, output, sizeof(output)) != 0)
            return 0;

        int count = 0;
        std::istringstream lines(output);
        std::string line;
        while (out && count < max_devices && std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.rfind("Device ", 0) != 0)
                continue;
            std::string rest = line.substr(7);
            size_t sp = rest.find(' ');
            if (sp == std::string::npos)
                continue;

            cp0_bt_device_t dev{};
            std::string addr = rest.substr(0, sp);
            std::string name = rest.substr(sp + 1);
            copy_string(dev.address, sizeof(dev.address), addr);
            copy_string(dev.name, sizeof(dev.name), name.empty() ? addr : name);
            dev.rssi = 0;
            dev.connected = 0;
            out[count++] = dev;
        }
        return count;
    }

    void time_str(char *buf, int buf_size) { fallback_time_str(buf, buf_size); }
#endif
};

} // namespace

extern "C" void init_settings(void)
{
    auto settings = std::make_shared<SettingsSystem>();
    cp0_signal_settings_api.append([settings](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        settings->api_call(std::move(arg), std::move(callback));
    });
}

extern "C" int cp0_backlight_read(void)
{
    return SettingsSystem::api_int({"BacklightRead"});
}

extern "C" int cp0_backlight_max(void)
{
    return SettingsSystem::api_int({"BacklightMax"}, 100);
}

extern "C" int cp0_backlight_write(int val)
{
    return SettingsSystem::api_int({"BacklightWrite", std::to_string(val)});
}

extern "C" cp0_bt_status_t cp0_bt_get_status(void)
{
    return SettingsSystem::api_bt_status();
}

extern "C" int cp0_bt_set_power(int on)
{
    return SettingsSystem::api_bt_power(on);
}

extern "C" int cp0_bt_scan(cp0_bt_device_t *out, int max_devices)
{
    return SettingsSystem::api_bt_scan(out, max_devices);
}

extern "C" void cp0_time_str(char *buf, int buf_size)
{
    SettingsSystem::api_time_str(buf, buf_size);
}
