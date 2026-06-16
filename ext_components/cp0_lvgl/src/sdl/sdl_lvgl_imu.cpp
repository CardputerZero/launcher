#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>

namespace {

void copy_cstr(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0)
        return;
    if (!src)
        src = "";
    std::strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

void fill_simulated_info(cp0_compass_info_t *info)
{
    if (!info)
        return;

    std::memset(info, 0, sizeof(*info));
    copy_cstr(info->status, sizeof(info->status), "SDL simulated IMU");
    info->yaw = 90.0f;
    info->pitch = 1.5f;
    info->roll = -2.0f;
    info->acc_x = 0.0f;
    info->acc_y = 0.0f;
    info->acc_z = 1.0f;
    info->gyr_x = 0.0f;
    info->gyr_y = 0.0f;
    info->gyr_z = 0.0f;
    info->mag_x = 32.0f;
    info->mag_y = 0.0f;
    info->mag_z = 4.0f;
    info->sensor_ready = 1;
}

class ImuSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty()) {
            report(callback, -1, "empty imu api\n");
            return;
        }

        if (arg.front() == "Read") {
            cp0_compass_info_t info{};
            int ret = read_info(&info);
            report(callback, ret, std::string(reinterpret_cast<const char *>(&info), sizeof(info)));
            return;
        }

        if (arg.front() == "Calibrate") {
            report(callback, 0, "SDL simulated compass calibration\n");
            return;
        }

        report(callback, -1, "unknown imu api\n");
    }

    int read_info(cp0_compass_info_t *info)
    {
        if (!info)
            return -1;
        fill_simulated_info(info);
        return 0;
    }

private:
    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }
};

std::shared_ptr<ImuSystem> g_imu;

} // namespace

extern "C" int cp0_compass_read(cp0_compass_read_cb_t callback, void *user)
{
    cp0_compass_info_t info{};
    int ret = -1;
    cp0_signal_imu_api({"Read"}, [&](int code, std::string data) {
        ret = code;
        if (data.size() == sizeof(info))
            std::memcpy(&info, data.data(), sizeof(info));
    });
    if (callback)
        callback(ret, &info, user);
    return ret;
}

extern "C" int cp0_compass_calibrate(void)
{
    int ret = -1;
    cp0_signal_imu_api({"Calibrate"}, [&](int code, std::string) {
        ret = code;
    });
    return ret;
}

extern "C" void init_imu(void)
{
    if (g_imu)
        return;

    g_imu = std::make_shared<ImuSystem>();
    cp0_signal_imu_api.append([](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        g_imu->api_call(std::move(arg), std::move(callback));
    });
}
