#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../cp0_app_internal_utils.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <list>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#if !defined(_WIN32)
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace {

struct IioDevicePaths {
    std::string accel;
    std::string magn;
    bool has_gyro = false;

    bool ready() const { return !accel.empty() && !magn.empty(); }
};

void clear_info(cp0_compass_info_t *info, const char *status, int ready)
{
    if (!info)
        return;
    std::memset(info, 0, sizeof(*info));
    cp0_copy_cstr(info->status, sizeof(info->status), status ? status : "IMU");
    info->sensor_ready = ready;
}

#if !defined(_WIN32)
bool file_exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool read_float_file(const std::string &path, float &out)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
        return false;
    ifs >> out;
    return !ifs.fail();
}

float read_float_file_or(const std::string &path, float fallback)
{
    float value = fallback;
    return read_float_file(path, value) ? value : fallback;
}

bool has_accel_files(const std::string &dir)
{
    return file_exists(dir + "/in_accel_x_raw") &&
           file_exists(dir + "/in_accel_y_raw") &&
           file_exists(dir + "/in_accel_z_raw");
}

bool has_magn_files(const std::string &dir)
{
    return file_exists(dir + "/in_magn_x_raw") &&
           file_exists(dir + "/in_magn_y_raw") &&
           file_exists(dir + "/in_magn_z_raw");
}

bool has_gyro_files(const std::string &dir)
{
    return file_exists(dir + "/in_anglvel_x_raw") &&
           file_exists(dir + "/in_anglvel_y_raw") &&
           file_exists(dir + "/in_anglvel_z_raw");
}

IioDevicePaths enumerate_iio_devices()
{
    static constexpr const char *kIioRoot = "/sys/bus/iio/devices";
    IioDevicePaths paths;

    DIR *dp = opendir(kIioRoot);
    if (!dp)
        return paths;

    while (dirent *ent = readdir(dp)) {
        if (std::strncmp(ent->d_name, "iio:device", 10) != 0)
            continue;

        std::string dir = std::string(kIioRoot) + "/" + ent->d_name;
        if (paths.accel.empty() && has_accel_files(dir)) {
            paths.accel = dir;
            paths.has_gyro = has_gyro_files(dir);
        }
        if (paths.magn.empty() && has_magn_files(dir))
            paths.magn = dir;
    }

    closedir(dp);
    return paths;
}

bool read_axis_triplet(const std::string &dir, const char *prefix,
                       float scale, float &x, float &y, float &z)
{
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    if (!read_float_file(dir + "/" + prefix + "_x_raw", rx)) return false;
    if (!read_float_file(dir + "/" + prefix + "_y_raw", ry)) return false;
    if (!read_float_file(dir + "/" + prefix + "_z_raw", rz)) return false;

    x = rx * scale;
    y = ry * scale;
    z = rz * scale;
    return true;
}
#endif

class ImuSystem {
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
            read(arg, callback);
            return;
        }

        if (arg.front() == "Calibrate") {
            calibrate(callback);
            return;
        }

        report(callback, -1, "unknown imu api\n");
    }

private:
    void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    void read(arg_t arg, callback_t callback)
    {
        (void)arg;
        cp0_compass_info_t info{};
        int ret = read_info(&info);
        report(callback, ret, std::string(reinterpret_cast<const char *>(&info), sizeof(info)));
    }

    void calibrate(callback_t callback)
    {
#if defined(_WIN32)
        report(callback, -1, "compass calibration unavailable\n");
#else
        if (calibrating_.exchange(true)) {
            report(callback, 1, "compass calibration already running\n");
            return;
        }

        std::thread([this]() {
            calibrate_worker();
            calibrating_.store(false);
        }).detach();
        report(callback, 0, "compass calibration started\n");
#endif
    }

    int read_info(cp0_compass_info_t *info)
    {
        if (!info)
            return -1;
        clear_info(info, "IIO sensor missing", 0);

#if defined(_WIN32)
        return -1;
#else
        IioDevicePaths paths = enumerate_iio_devices();
        if (!paths.ready())
            return -1;

        const float acc_scale = read_float_file_or(paths.accel + "/in_accel_scale", 1.0f);
        const float gyr_scale = read_float_file_or(paths.accel + "/in_anglvel_scale", 1.0f);
        const float mag_scale = read_float_file_or(paths.magn + "/in_magn_scale", 1.0f);

        float acc_x = 0.0f, acc_y = 0.0f, acc_z = 0.0f;
        float mag_x = 0.0f, mag_y = 0.0f, mag_z = 0.0f;
        float gyr_x = 0.0f, gyr_y = 0.0f, gyr_z = 0.0f;

        if (!read_axis_triplet(paths.accel, "in_accel", acc_scale, acc_x, acc_y, acc_z) ||
            !read_axis_triplet(paths.magn, "in_magn", mag_scale, mag_x, mag_y, mag_z)) {
            clear_info(info, "IIO read failed", 0);
            return -1;
        }

        if (paths.has_gyro)
            read_axis_triplet(paths.accel, "in_anglvel", gyr_scale, gyr_x, gyr_y, gyr_z);

        apply_mag_bias(mag_x, mag_y, mag_z);

        float pitch = std::atan2(-acc_x, std::sqrt(acc_y * acc_y + acc_z * acc_z));
        float roll = std::atan2(acc_y, acc_z);
        float sin_p = std::sin(pitch);
        float cos_p = std::cos(pitch);
        float sin_r = std::sin(roll);
        float cos_r = std::cos(roll);

        float mag_x_h = mag_x * cos_p + mag_z * sin_p;
        float mag_y_h = mag_x * sin_r * sin_p + mag_y * cos_r - mag_z * sin_r * cos_p;
        float yaw = std::atan2(-mag_y_h, mag_x_h) * 180.0f / 3.1415926f;
        if (yaw < 0.0f)
            yaw += 360.0f;

        clear_info(info, calibrating_.load() ? "Calibrating..." : "Sensor OK", 1);
        info->yaw = yaw;
        info->pitch = pitch * 180.0f / 3.1415926f;
        info->roll = roll * 180.0f / 3.1415926f;
        info->acc_x = acc_x;
        info->acc_y = acc_y;
        info->acc_z = acc_z;
        info->gyr_x = gyr_x;
        info->gyr_y = gyr_y;
        info->gyr_z = gyr_z;
        info->mag_x = mag_x;
        info->mag_y = mag_y;
        info->mag_z = mag_z;
        return 0;
#endif
    }

#if !defined(_WIN32)
    void apply_mag_bias(float &mag_x, float &mag_y, float &mag_z)
    {
        std::lock_guard<std::mutex> lock(bias_mutex_);
        if (!mag_bias_valid_)
            return;

        mag_x -= mag_bias_x_;
        mag_y -= mag_bias_y_;
        mag_z -= mag_bias_z_;
    }

    void calibrate_worker()
    {
        IioDevicePaths paths = enumerate_iio_devices();
        if (!paths.ready())
            return;

        const float mag_scale = read_float_file_or(paths.magn + "/in_magn_scale", 1.0f);
        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float min_z = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();
        float max_z = std::numeric_limits<float>::lowest();
        int samples = 0;

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            float mag_x = 0.0f;
            float mag_y = 0.0f;
            float mag_z = 0.0f;
            if (read_axis_triplet(paths.magn, "in_magn", mag_scale, mag_x, mag_y, mag_z)) {
                min_x = std::min(min_x, mag_x);
                min_y = std::min(min_y, mag_y);
                min_z = std::min(min_z, mag_z);
                max_x = std::max(max_x, mag_x);
                max_y = std::max(max_y, mag_y);
                max_z = std::max(max_z, mag_z);
                samples++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (samples < 10)
            return;

        std::lock_guard<std::mutex> lock(bias_mutex_);
        mag_bias_x_ = (min_x + max_x) / 2.0f;
        mag_bias_y_ = (min_y + max_y) / 2.0f;
        mag_bias_z_ = (min_z + max_z) / 2.0f;
        mag_bias_valid_ = true;
    }
#endif

    std::atomic<bool> calibrating_{false};
    std::mutex bias_mutex_;
    bool mag_bias_valid_ = false;
    float mag_bias_x_ = 0.0f;
    float mag_bias_y_ = 0.0f;
    float mag_bias_z_ = 0.0f;
};

} // namespace

extern "C" int cp0_compass_read(cp0_compass_read_cb_t callback, void *user)
{
    cp0_compass_info_t info{};
    clear_info(&info, "IMU unavailable", 0);
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
    std::shared_ptr<ImuSystem> imu = std::make_shared<ImuSystem>();
    cp0_signal_imu_api.append([imu](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        imu->api_call(std::move(arg), std::move(callback));
    });
}
