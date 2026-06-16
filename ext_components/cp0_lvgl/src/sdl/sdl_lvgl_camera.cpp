#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
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

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty()) {
            report(callback, -1, "empty camera api\n");
            return;
        }

        const std::string command = arg.front();
        if (command == "SetCallback") {
            std::lock_guard<std::mutex> lock(mutex_);
            status_callback_ = std::move(callback);
            report_locked(status_callback_, 0, "camera callback set\n");
            return;
        }
        if (command == "SetFrameCallback") {
            std::lock_guard<std::mutex> lock(mutex_);
            frame_callback_ = std::move(callback);
            report_locked(frame_callback_, 0, "camera frame callback set\n");
            return;
        }
        if (command == "Open" || command == "Start") {
            const int width = to_int(nth_arg(arg, 1), 320);
            const int height = to_int(nth_arg(arg, 2), 150);
            open_camera(width, height);
            report(callback, 0, "SDL camera open\n");
            return;
        }
        if (command == "Close" || command == "Stop") {
            close_camera();
            report(callback, 0, "camera close\n");
            return;
        }
        if (command == "Capture" || command == "Photo") {
            const std::string path = nth_arg(arg, 1);
            const int width = to_int(nth_arg(arg, 2), width_);
            const int height = to_int(nth_arg(arg, 3), height_);
            const int ret = capture(path, width, height);
            report(callback, ret, ret == 0 ? path + "\n" : "camera capture failed\n");
            return;
        }
        if (command == "Status") {
            report(callback, running_.load() ? 0 : 1, running_.load() ? "camera streaming\n" : "camera stopped\n");
            return;
        }
        if (command == "ZoomIn") {
            std::lock_guard<std::mutex> lock(mutex_);
            zoom_percent_ = zoom_percent_ < 250 ? 250 : 500;
            report_locked(callback, 0, zoom_payload_locked());
            return;
        }
        if (command == "ZoomOut") {
            std::lock_guard<std::mutex> lock(mutex_);
            zoom_percent_ = zoom_percent_ > 250 ? 250 : 100;
            if (zoom_percent_ == 100)
                view_x_percent_ = view_y_percent_ = 50;
            report_locked(callback, 0, zoom_payload_locked());
            return;
        }
        if (command == "Pan") {
            std::lock_guard<std::mutex> lock(mutex_);
            const int dx = to_int(nth_arg(arg, 1), 0);
            const int dy = to_int(nth_arg(arg, 2), 0);
            view_x_percent_ = clamp(view_x_percent_ + dx * 8, 0, 100);
            view_y_percent_ = clamp(view_y_percent_ + dy * 8, 0, 100);
            report_locked(callback, 0, zoom_payload_locked());
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
    int width_ = 320;
    int height_ = 150;
    int zoom_percent_ = 100;
    int view_x_percent_ = 50;
    int view_y_percent_ = 50;
    uint32_t frame_index_ = 0;

    static int clamp(int value, int lo, int hi)
    {
        return std::max(lo, std::min(hi, value));
    }

    static std::string nth_arg(const arg_t &arg, size_t n)
    {
        if (arg.size() <= n)
            return "";
        auto it = arg.begin();
        std::advance(it, n);
        return *it;
    }

    static int to_int(const std::string &value, int fallback)
    {
        if (value.empty())
            return fallback;
        char *end = nullptr;
        long ret = std::strtol(value.c_str(), &end, 10);
        return end && *end == '\0' ? static_cast<int>(ret) : fallback;
    }

    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
    {
        return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
    }

    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    static void report_locked(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    std::string zoom_payload_locked() const
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "ZOOM %d %d %d\n", zoom_percent_, view_x_percent_, view_y_percent_);
        return buf;
    }

    void open_camera(int width, int height)
    {
        close_camera();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            width_ = std::max(1, width);
            height_ = std::max(1, height);
            frame_index_ = 0;
        }
        running_.store(true);
        worker_ = std::thread([this]() { frame_loop(); });
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
            if (callback)
                callback(0, payload);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
    }

    std::string make_frame_payload_locked()
    {
        std::vector<uint16_t> pixels(static_cast<size_t>(width_) * height_);
        const uint32_t tick = frame_index_++;
        const int pan_x = (view_x_percent_ - 50) * 2;
        const int pan_y = (view_y_percent_ - 50) * 2;
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                int sx = (x * zoom_percent_) / 100 + pan_x + static_cast<int>(tick);
                int sy = (y * zoom_percent_) / 100 + pan_y;
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
                    static_cast<uint8_t>((x + y + frame_index_) & 0xff),
                };
                std::fwrite(rgb, 1, sizeof(rgb), file);
            }
        }
        return std::fclose(file) == 0 ? 0 : -1;
    }
};

} // namespace

extern "C" void init_camera(void)
{
    std::shared_ptr<CameraSystem> camera = std::make_shared<CameraSystem>();
    cp0_signal_camera_api.append([camera](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        camera->api_call(std::move(arg), std::move(callback));
    });
}
