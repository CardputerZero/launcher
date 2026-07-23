#include "hal_lvgl_bsp.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <list>
#include <string>
#include <thread>

eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_camera_api;

extern "C" void init_camera(void);
extern "C" void deinit_camera(void) noexcept;

int main()
{
    static_assert(noexcept(deinit_camera()));
    init_camera();

    std::atomic<int> frames{0};
    cp0_signal_camera_api({"SetFrameCallback"},
        [&](int, std::string) { ++frames; });
    cp0_signal_camera_api({"Start", "8", "8"}, nullptr);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (frames.load() < 2 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(frames.load() >= 2);

    deinit_camera();
    const int stopped_frames = frames.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    assert(frames.load() == stopped_frames);
    int callbacks = 0;
    cp0_signal_camera_api({"Status"}, [&](int, std::string) { ++callbacks; });
    assert(callbacks == 0);

    init_camera();
    cp0_signal_camera_api({"Status"}, [&](int code, std::string) {
        ++callbacks;
        assert(code == 1);
    });
    assert(callbacks == 1);
    deinit_camera();
}
