#include "hal_lvgl_bsp.h"
#include "../cp0/cp0_camera_viewport.hpp"
#include "../cp0_camera_api_contract.hpp"
#include "../cp0_callback_contract.hpp"
#include "../cp0_signal_registration.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

class CameraSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    ~CameraSystem()
    {
        close_camera();
    }

    void shutdown()
    {
        close_camera();
        std::lock_guard<std::mutex> lock(mutex_);
        status_callback_ = {};
        frame_callback_ = {};
    }

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty()) {
            report(callback, -1, "empty camera api\n");
            return;
        }

        const std::string command = arg.front();
        if (command == "SetCallback") {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                status_callback_ = callback;
            }
            report(std::move(callback), 0, "camera callback set\n");
            return;
        }
        if (command == "SetFrameCallback") {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                frame_callback_ = callback;
            }
            report(std::move(callback), 0, "camera frame callback set\n");
            return;
        }
        if (command == "Open" || command == "Start") {
            const int width = cp0_camera_api::parse_integer_argument(
                nth_arg(arg, 1), cp0_camera_api::DEFAULT_WIDTH);
            const int height = cp0_camera_api::parse_integer_argument(
                nth_arg(arg, 2), cp0_camera_api::DEFAULT_HEIGHT);
            const int result = open_camera(width, height);
            report(callback, result, result == 0 ? "camera open\n" : "camera open failed\n");
            return;
        }
        if (command == "Close" || command == "Stop") {
            close_camera();
            report(callback, 0, "camera close\n");
            return;
        }
        if (command == "Capture" || command == "Photo") {
            const std::string path = nth_arg(arg, 1);
            const int width = cp0_camera_api::parse_integer_argument(
                nth_arg(arg, 2), cp0_camera_api::DEFAULT_WIDTH);
            const int height = cp0_camera_api::parse_integer_argument(
                nth_arg(arg, 3), cp0_camera_api::DEFAULT_HEIGHT);
            const int ret = capture(path, width, height);
            report(callback, ret, ret == 0 ? path + "\n" : "camera capture failed\n");
            return;
        }
        if (command == "Status") {
            report(callback, running_.load() ? 0 : 1, running_.load() ? "camera streaming\n" : "camera stopped\n");
            return;
        }
        if (command == "ZoomIn") {
            std::string payload;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                viewport_.zoom_in();
                payload = viewport_.status_text();
            }
            report(std::move(callback), 0, payload);
            return;
        }
        if (command == "ZoomOut") {
            std::string payload;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                viewport_.zoom_out();
                payload = viewport_.status_text();
            }
            report(std::move(callback), 0, payload);
            return;
        }
        if (command == "Pan") {
            const int dx = cp0_camera_api::parse_integer_argument(nth_arg(arg, 1), 0);
            const int dy = cp0_camera_api::parse_integer_argument(nth_arg(arg, 2), 0);
            std::string payload;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                viewport_.pan(dx, dy);
                payload = viewport_.status_text();
            }
            report(std::move(callback), 0, payload);
            return;
        }

        report(callback, -1, "unknown camera api\n");
    }

private:
    callback_t status_callback_;
    callback_t frame_callback_;
    std::mutex mutex_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    int width_ = cp0_camera_api::DEFAULT_WIDTH;
    int height_ = cp0_camera_api::DEFAULT_HEIGHT;
    cp0_camera::Viewport viewport_;
    uint32_t frame_index_ = 0;

    static std::string nth_arg(const arg_t &arg, size_t n)
    {
        if (arg.size() <= n)
            return "";
        auto it = arg.begin();
        std::advance(it, n);
        return *it;
    }

    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
    {
        return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
    }

    void report(callback_t callback, int code, const std::string &data)
    {
        if (!callback) {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = status_callback_;
        }
        cp0::callback::invoke(callback, code, data);
    }

    int open_camera(int width, int height)
    {
        close_camera();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            width_ = std::max(1, width);
            height_ = std::max(1, height);
            frame_index_ = 0;
        }
        running_.store(true);
        try {
            worker_ = std::thread([this]() { frame_loop(); });
        } catch (...) {
            running_.store(false);
            return -2;
        }
        return 0;
    }

    void close_camera()
    {
        running_.store(false);
        if (worker_.joinable())
            worker_.join();
    }

    void frame_loop()
    {
        while (running_.load()) {
            callback_t callback;
            std::string payload;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                callback = frame_callback_;
                if (callback)
                    payload = make_frame_payload_locked();
            }
            cp0::callback::invoke(callback, 0, payload);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
    }

    std::string make_frame_payload_locked()
    {
        std::vector<uint16_t> pixels(static_cast<size_t>(width_) * height_);
        const uint32_t tick = frame_index_++;
        const int zoom_percent = viewport_.zoom_percent();
        const int pan_x = (viewport_.view_x_percent() - 50) * 2;
        const int pan_y = (viewport_.view_y_percent() - 50) * 2;
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                int sx = (x * zoom_percent) / 100 + pan_x + static_cast<int>(tick);
                int sy = (y * zoom_percent) / 100 + pan_y;
                uint8_t r = static_cast<uint8_t>((sx * 255) / std::max(1, width_));
                uint8_t g = static_cast<uint8_t>((sy * 255) / std::max(1, height_));
                uint8_t b = static_cast<uint8_t>((sx + sy + static_cast<int>(tick) * 3) & 0xff);
                if (((x / 16) + (y / 16) + (tick / 8)) & 1)
                    b = static_cast<uint8_t>(255 - b);
                pixels[static_cast<size_t>(y) * width_ + x] = rgb565(r, g, b);
            }
        }

        char header[64];
        const int header_len = std::snprintf(header, sizeof(header), "FRAME %d %d RGB565\n", width_, height_);
        std::string payload(header, header_len);
        payload.append(reinterpret_cast<const char *>(pixels.data()), pixels.size() * sizeof(uint16_t));
        return payload;
    }

    int capture(const std::string &path, int width, int height)
    {
        if (path.empty())
            return -1;
        uint32_t frame_index = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            frame_index = frame_index_;
        }
        FILE *file = std::fopen(path.c_str(), "wb");
        if (!file)
            return -1;

        width = std::max(1, width);
        height = std::max(1, height);
        std::fprintf(file, "P6\n%d %d\n255\n", width, height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t rgb[3] = {
                    static_cast<uint8_t>((x * 255) / width),
                    static_cast<uint8_t>((y * 255) / height),
                    static_cast<uint8_t>((x + y + frame_index) & 0xff),
                };
                std::fwrite(rgb, 1, sizeof(rgb), file);
            }
        }
        return std::fclose(file) == 0 ? 0 : -1;
    }
};

} // namespace

namespace {

std::shared_ptr<CameraSystem> &camera_system()
{
    static auto camera = std::make_shared<CameraSystem>();
    return camera;
}

cp0::SignalRegistration<decltype(cp0_signal_camera_api)> &camera_registration()
{
    static cp0::SignalRegistration<decltype(cp0_signal_camera_api)> registration;
    return registration;
}

} // namespace

extern "C" void init_camera(void)
{
    const auto camera = camera_system();
    camera_registration().replace(cp0_signal_camera_api,
                         [camera](std::list<std::string> arg,
                                  std::function<void(int, std::string)> callback) {
                             try {
                                 camera->api_call(std::move(arg), callback);
                             } catch (...) {
                                 cp0::callback::invoke(
                                     callback, -1, "camera backend failure\n");
                             }
                         });
}

extern "C" void deinit_camera(void) noexcept
{
    try {
        camera_registration().reset();
    } catch (...) {
    }
    try {
        camera_system()->shutdown();
    } catch (...) {
    }
}
