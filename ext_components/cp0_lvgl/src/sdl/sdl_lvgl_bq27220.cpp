#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <cstdlib>
#include <ctime>
#include <functional>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace {

cp0_battery_info_t simulated_battery_info()
{
    cp0_battery_info_t info{};
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
        return simulated_battery_info();
    }

    int calibrate(int command_index)
    {
        return (command_index >= 0 && command_index < 4) ? 0 : -1;
    }

    static bool decode_info(const std::string &data, cp0_battery_info_t *info)
    {
        if (!info)
            return false;
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

private:
    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        for (size_t i = 0; i < index && it != arg.end(); ++i)
            ++it;
        return it == arg.end() ? std::string() : *it;
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
};

std::shared_ptr<Bq27220System> g_bq27220;

} // namespace

extern "C" {

cp0_battery_info_t cp0_battery_read(void)
{
    cp0_battery_info_t info{};
    cp0_signal_bq27220_api({"Read"}, [&](int code, std::string data) {
        if (code == 0)
            Bq27220System::decode_info(data, &info);
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
    if (g_bq27220)
        return;

    g_bq27220 = std::make_shared<Bq27220System>();
    cp0_signal_bq27220_api.append([](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        g_bq27220->api_call(std::move(arg), std::move(callback));
    });
}

}
