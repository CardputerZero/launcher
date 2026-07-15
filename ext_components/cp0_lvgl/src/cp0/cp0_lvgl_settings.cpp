#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"

#include "cp0_lvgl_log.h"

#include <algorithm>
#include <cerrno>
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
#include <string>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <unistd.h>
#include <utility>

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
static constexpr const char *kGrove5vLedPaths[] = {
    "/sys/class/leds/grove_5v_out/brightness",
};
static constexpr const char *kExt5vLedPaths[] = {
    "/sys/class/leds/ext_5v_out/brightness",
};

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
        } else if (cmd == "Log") {
            const std::string topic = nth_arg(arg, 1);
            const std::string message = nth_arg(arg, 2);
            settings_log(topic.c_str(), message.c_str());
            report(callback, 0, "");
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
        if (is_grove5v_name(name)) {
            int ret = write_led_value(kGrove5vLedPaths, val);
            if (ret != -ENOENT)
                return ret;
            const ExtPortGpioMap &map = active_gpio_map_locked();
            return set_gpio_value(map.chip_path, map.grove5v_line, map.grove5v_active_low, "GROVE5V", val);
        }
        if (is_ext5v_name(name)) {
            int ret = write_led_value(kExt5vLedPaths, val);
            if (ret != -ENOENT)
                return ret;
            const ExtPortGpioMap &map = active_gpio_map_locked();
            return set_gpio_value(map.chip_path, map.ext5v_line, map.ext5v_active_low, "EXT5V", val);
        }
        return -EINVAL;
    }

    int get_named_gpio(const char *name)
    {
        std::lock_guard<std::mutex> lock(gpio_mutex_);
        if (is_grove5v_name(name)) {
            int value = read_led_value(kGrove5vLedPaths);
            if (value != -ENOENT)
                return value;
            const ExtPortGpioMap &map = active_gpio_map_locked();
            return get_gpio_value(map.chip_path, map.grove5v_line, map.grove5v_active_low, "GROVE5V");
        }
        if (is_ext5v_name(name)) {
            int value = read_led_value(kExt5vLedPaths);
            if (value != -ENOENT)
                return value;
            const ExtPortGpioMap &map = active_gpio_map_locked();
            return get_gpio_value(map.chip_path, map.ext5v_line, map.ext5v_active_low, "EXT5V");
        }
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

    template <size_t N>
    static int write_led_value(const char *const (&paths)[N], int val)
    {
        const char value = val ? '1' : '0';
        for (const char *path : paths) {
            int fd = open(path, O_WRONLY | O_CLOEXEC);
            if (fd < 0) {
                if (errno == ENOENT)
                    continue;
                return -errno;
            }
            ssize_t written = write(fd, &value, 1);
            int err = written == 1 ? 0 : (written < 0 ? errno : EIO);
            close(fd);
            return -err;
        }
        return -ENOENT;
    }

    template <size_t N>
    static int read_led_value(const char *const (&paths)[N])
    {
        for (const char *path : paths) {
            int fd = open(path, O_RDONLY | O_CLOEXEC);
            if (fd < 0) {
                if (errno == ENOENT)
                    continue;
                return -errno;
            }
            char buf[16] = {};
            ssize_t size = read(fd, buf, sizeof(buf) - 1);
            int err = size >= 0 ? 0 : errno;
            close(fd);
            if (err != 0)
                return -err;
            if (size == 0)
                return -EIO;
            return std::strtol(buf, nullptr, 10) != 0 ? 1 : 0;
        }
        return -ENOENT;
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

    void settings_log(const char *topic, const char *message)
    {
        cp0_zmq_log(topic && topic[0] ? topic : "settings", message ? message : "");
    }

    void time_str(char *buf, int buf_size) { fallback_time_str(buf, buf_size); }
};

} // namespace

extern "C" void init_settings(void)
{
    cp0_zmq_log_init();
    cp0_zmq_log("settings", "init_settings");
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

extern "C" void cp0_time_str(char *buf, int buf_size)
{
    SettingsSystem::api_time_str(buf, buf_size);
}
