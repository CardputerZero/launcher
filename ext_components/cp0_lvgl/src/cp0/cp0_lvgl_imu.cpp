#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../cp0_app_internal_utils.h"
#include "../cp0_callback_contract.hpp"
#include "../cp0_imu_codec.hpp"
#include "../cp0_imu_worker_contract.hpp"
#include "../cp0_signal_registration.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
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

    ~ImuSystem() { stop(); }

    void stop() noexcept
    {
        std::thread worker;
        {
            std::lock_guard<std::mutex> lock(worker_mutex_);
            stopping_ = true;
            cancel_calibration_.store(true);
            calibration_wake_.notify_all();
            if (calibration_worker_.joinable())
                worker = std::move(calibration_worker_);
        }
        cp0::imu::cancel_and_join_worker(worker, [] {}, [] {});
        calibrating_.store(false);
    }

    void api_call(arg_t arg, callback_t callback)
    {
        try {
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
        } catch (...) {
            report(callback, -1, "imu api failure\n");
        }
    }

private:
    void report(const callback_t &callback, int code, const std::string &data)
    {
        cp0::callback::invoke(callback, code, data);
    }

    void read(arg_t arg, callback_t callback)
    {
        (void)arg;
        cp0_compass_info_t info{};
        int ret = read_info(&info);
        report(callback, ret, cp0::imu::encode_info(info));
    }

    void calibrate(callback_t callback)
    {
#if defined(_WIN32)
        report(callback, -1, "compass calibration unavailable\n");
#else
        cp0::imu::prepare_then_report(
            [this] {
                std::lock_guard<std::mutex> lock(worker_mutex_);
                if (stopping_) return cp0::imu::StartOutcome::Stopping;
                if (calibrating_.exchange(true))
                    return cp0::imu::StartOutcome::AlreadyRunning;
                try {
                    cp0::imu::reap_completed_worker(calibration_worker_, false);
                    cancel_calibration_.store(false);
                    calibration_worker_ = std::thread([this]() {
                        try {
                            calibrate_worker();
                        } catch (...) {
                        }
                        calibrating_.store(false);
                    });
                    return cp0::imu::StartOutcome::Started;
                } catch (...) {
                    calibrating_.store(false);
                    return cp0::imu::StartOutcome::ThreadCreationFailed;
                }
            },
            [this, &callback](cp0::imu::StartOutcome outcome) {
                if (outcome == cp0::imu::StartOutcome::Started)
                    report(callback, 0, "compass calibration started\n");
                else if (outcome == cp0::imu::StartOutcome::AlreadyRunning)
                    report(callback, 1, "compass calibration already running\n");
                else if (outcome == cp0::imu::StartOutcome::Stopping)
                    report(callback, -1, "compass calibration stopping\n");
                else
                    report(callback, -1, "compass calibration unavailable\n");
            });
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
        while (!cancel_calibration_.load() &&
               std::chrono::steady_clock::now() < deadline) {
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
            std::unique_lock<std::mutex> lock(calibration_wait_mutex_);
            calibration_wake_.wait_for(lock, std::chrono::milliseconds(20),
                [this] { return cancel_calibration_.load(); });
        }

        if (cancel_calibration_.load() || samples < 10)
            return;

        std::lock_guard<std::mutex> lock(bias_mutex_);
        mag_bias_x_ = (min_x + max_x) / 2.0f;
        mag_bias_y_ = (min_y + max_y) / 2.0f;
        mag_bias_z_ = (min_z + max_z) / 2.0f;
        mag_bias_valid_ = true;
    }
#endif

    std::atomic<bool> calibrating_{false};
    std::atomic<bool> cancel_calibration_{false};
    std::mutex worker_mutex_;
    std::mutex calibration_wait_mutex_;
    std::condition_variable calibration_wake_;
    std::thread calibration_worker_;
    bool stopping_ = false;
    std::mutex bias_mutex_;
    bool mag_bias_valid_ = false;
    float mag_bias_x_ = 0.0f;
    float mag_bias_y_ = 0.0f;
    float mag_bias_z_ = 0.0f;
};

using ImuRegistration = cp0::SignalRegistration<decltype(cp0_signal_imu_api)>;

struct ImuRuntime {
    std::mutex mutex;
    ImuRegistration registration;
    std::shared_ptr<ImuSystem> service;
};

ImuRuntime &imu_runtime()
{
    static ImuRuntime runtime;
    return runtime;
}

} // namespace

extern "C" int cp0_compass_read(cp0_compass_read_cb_t callback, void *user)
{
    cp0_compass_info_t info{};
    clear_info(&info, "IMU unavailable", 0);
    int ret = -1;
    try {
        cp0_signal_imu_api({"Read"}, [&](int code, std::string data) {
            ret = code;
            if (!cp0::imu::decode_info(data, info))
                ret = -1;
        });
    } catch (...) {
        ret = -1;
    }
    cp0::callback::invoke_direct(callback, ret, &info, user);
    return ret;
}

extern "C" int cp0_compass_calibrate(void)
{
    int ret = -1;
    try {
        cp0_signal_imu_api({"Calibrate"}, [&](int code, std::string) {
            ret = code;
        });
    } catch (...) {
        ret = -1;
    }
    return ret;
}

extern "C" void init_imu(void)
{
    try {
        ImuRuntime &runtime = imu_runtime();
        std::lock_guard<std::mutex> lock(runtime.mutex);
        if (runtime.service) return;
        auto imu = std::make_shared<ImuSystem>();
        if (!runtime.registration.replace(
                cp0_signal_imu_api,
                [imu](std::list<std::string> arg,
                      std::function<void(int, std::string)> callback) {
                    imu->api_call(std::move(arg), std::move(callback));
                }))
            return;
        runtime.service = std::move(imu);
    } catch (...) {
    }
}

extern "C" void deinit_imu(void)
{
    try {
        ImuRuntime &runtime = imu_runtime();
        std::shared_ptr<ImuSystem> imu;
        {
            std::lock_guard<std::mutex> lock(runtime.mutex);
            runtime.registration.reset();
            imu = std::move(runtime.service);
        }
        if (imu) imu->stop();
    } catch (...) {
    }
}
