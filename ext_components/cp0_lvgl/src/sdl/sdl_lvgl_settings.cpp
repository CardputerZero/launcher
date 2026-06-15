#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"

#ifdef HAL_PLATFORM_SDL
#include "hal/hal_settings.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern "C" time_t cp0_sdl_time_now(void);

namespace {

class SettingsSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

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
    void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
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

extern "C" int hal_backlight_read(void) { return 75; }
extern "C" int hal_backlight_max(void) { return 100; }
extern "C" int hal_backlight_write(int val)
{
    return std::max(0, std::min(hal_backlight_max(), val));
}

extern "C" hal_battery_info_t hal_battery_read(void)
{
    hal_battery_info_t info{};
    cp0_battery_info_t cp0 = cp0_battery_read();
    info.voltage_mv = cp0.voltage_mv;
    info.current_ma = cp0.current_ma;
    info.temperature_c10 = cp0.temperature_c10;
    info.soc = cp0.soc;
    info.remain_mah = cp0.remain_mah;
    info.full_mah = cp0.full_mah;
    info.flags = cp0.flags;
    info.avg_current_ma = cp0.avg_current_ma;
    info.valid = cp0.valid;
    return info;
}

extern "C" int hal_volume_read(void) { return 39; }
extern "C" int hal_volume_write(int val) { return std::max(0, std::min(63, val)); }

extern "C" hal_wifi_status_t hal_wifi_get_status(void)
{
    hal_wifi_status_t st{};
    st.connected = 1;
    std::strncpy(st.ssid, "SimulatedWiFi", WIFI_SSID_MAX - 1);
    std::strncpy(st.ip, "192.168.1.100", sizeof(st.ip) - 1);
    st.signal = 80;
    return st;
}

extern "C" int hal_wifi_scan(hal_wifi_ap_t *out, int max_aps)
{
    if (!out || max_aps <= 0)
        return 0;

    const int count = std::min(max_aps, 3);
    std::memset(out, 0, sizeof(hal_wifi_ap_t) * static_cast<size_t>(count));
    if (count > 0) {
        std::strncpy(out[0].ssid, "SimulatedWiFi", WIFI_SSID_MAX - 1);
        std::strncpy(out[0].security, "WPA2", sizeof(out[0].security) - 1);
        out[0].signal = 80;
        out[0].in_use = 1;
    }
    if (count > 1) {
        std::strncpy(out[1].ssid, "Neighbor_5G", WIFI_SSID_MAX - 1);
        std::strncpy(out[1].security, "WPA2", sizeof(out[1].security) - 1);
        out[1].signal = 55;
    }
    if (count > 2) {
        std::strncpy(out[2].ssid, "FreeWiFi", WIFI_SSID_MAX - 1);
        std::strncpy(out[2].security, "Open", sizeof(out[2].security) - 1);
        out[2].signal = 30;
    }
    return count;
}

extern "C" int hal_wifi_connect(const char *ssid, const char *password)
{
    (void)ssid;
    (void)password;
    return 0;
}

extern "C" int hal_wifi_disconnect(void) { return 0; }

extern "C" hal_bt_status_t hal_bt_get_status(void)
{
    hal_bt_status_t st{};
    st.powered = 0;
    std::strncpy(st.address, "00:00:00:00:00:00", sizeof(st.address) - 1);
    return st;
}

extern "C" int hal_bt_set_power(int on)
{
    (void)on;
    return 0;
}

extern "C" int hal_bt_scan(hal_bt_device_t *out, int max_devices)
{
    (void)out;
    (void)max_devices;
    return 0;
}

extern "C" void hal_time_str(char *buf, int buf_size)
{
    if (!buf || buf_size <= 0)
        return;
    std::time_t now = cp0_sdl_time_now();
    std::tm *t = std::localtime(&now);
    if (!t) {
        buf[0] = '\0';
        return;
    }
    std::snprintf(buf, static_cast<size_t>(buf_size), "%02d:%02d", t->tm_hour, t->tm_min);
}
