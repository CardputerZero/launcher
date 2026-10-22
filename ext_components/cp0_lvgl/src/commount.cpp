/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "hal_lvgl_bsp.h"
#include "commount.h"
#include "cp0_lvgl_app.h"
#include "cp0_network_api_contract.hpp"
#include "cp0_integer_codec.hpp"
#include "cp0_sync_signal.hpp"
#include "cp0/cp0_audio_mixer_control.hpp"
#include "lvgl/lvgl.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#define def_hal_fun(arg, name) eventpp::CallbackList<arg> name;
#include "signal_register_plan.h"
#undef def_hal_fun

eventpp::EventQueue<CP0_C_EVENT_t, void(const std::list<std::string>)> cp0_task_queue;

extern "C" int cp0_wifi_status_read(cp0_wifi_status_t *status)
{
    if (!status)
        return -1;

    *status = {};
    int result = -1;
    cp0::signal::invoke_noexcept([&] { cp0_signal_wifi_api({"Status"}, [&](int code, std::string data) {
        if (code != 0 || !cp0::network::decode_status_payload(data, *status)) {
            result = code != 0 ? code : -1;
            return;
        }
        result = 0;
    }); });
    return result;
}

namespace {

int wifi_command(const std::list<std::string> &args)
{
    int result = -1;
    cp0::signal::invoke_noexcept([&] {
        cp0_signal_wifi_api(args, [&](int code, std::string) { result = code; });
    });
    return result;
}

} // namespace

extern "C" int cp0_wifi_scan(cp0_wifi_ap_t *entries, int max_entries)
{
    if (!entries || max_entries <= 0)
        return 0;
    std::fill_n(entries, static_cast<size_t>(max_entries), cp0_wifi_ap_t{});
    const int request_limit = std::min(max_entries, CP0_WIFI_AP_MAX);
    int count = CP0_WIFI_ERROR_SERVICE;
    cp0::signal::invoke_noexcept([&] { cp0_signal_wifi_api({"Scan", std::to_string(request_limit)}, [&](int code, std::string data) {
        if (code < 0) {
            count = code;
            return;
        }
        count = cp0::network::decode_scan_payload(data, entries, max_entries);
    }); });
    return count;
}

extern "C" int cp0_wifi_connect(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0])
        return -1;
    return password && password[0] ? wifi_command({"Connect", ssid, password})
                                   : wifi_command({"Connect", ssid});
}

extern "C" int cp0_wifi_connect_hidden(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0])
        return -1;
    return password && password[0] ? wifi_command({"ConnectHidden", ssid, password})
                                   : wifi_command({"ConnectHidden", ssid});
}

extern "C" int cp0_wifi_profile_forget(const char *ssid)
{
    return ssid && ssid[0] ? wifi_command({"ProfileForget", ssid}) : -1;
}

extern "C" int cp0_wifi_profile_exists(const char *ssid)
{
    return ssid && ssid[0] ? wifi_command({"ProfileExists", ssid}) : 0;
}

extern "C" int cp0_wifi_disconnect_active(void)
{
    return wifi_command({"ProfileDisconnectActive"});
}

extern "C" int cp0_wifi_radio_enabled(void)
{
    return wifi_command({"RadioEnabled"});
}

extern "C" int cp0_wifi_radio_set_enabled(int enabled)
{
    return wifi_command({"RadioSetEnabled", enabled ? "on" : "off"});
}
static std::atomic_bool queue_run_flage{true};
extern "C" void init_lvgl_event_cpp()
{
    // cp0_task_queue.appendListener(CP0_C_EVENT_END, [](const std::list<std::string> args)
    //                               { queue_run_flage = false; });
    // std::thread t([]()
    //               {
    //     while (queue_run_flage.load()) {
    //         cp0_task_queue.wait();
    //         cp0_task_queue.process();
    //     } });
    // t.detach();
}

static int config_get_int(const char *key, int default_val)
{
    int val = default_val;
    cp0::signal::invoke_noexcept([&] { cp0_signal_config_api({"GetInt", key ? std::string(key) : std::string(), std::to_string(default_val)},
                          [&](int code, std::string data) {
                              int parsed = 0;
                              if (code == 0 && cp0_integer_codec::parse_int(data, parsed))
                                  val = parsed;
                          }); });
    return val;
}

// PipeWire starts alongside the launcher, so at boot the speaker sink may not
// exist yet or may still sit behind the "auto_null" placeholder, whose volume
// is thrown away when the hardware sink is created. Once the sink appears,
// WirePlumber also reapplies the volume it stored for the speaker port, which
// can land after an early write. So the saved volume is rewritten until it
// reads back from a hardware sink and has held there for the settle period.
// The saved value is re-read on every pass so a change made in the UI wins.
// The deadline covers a cold boot, where the sink can show up seconds later.
// Retries run off the init thread: the display must not wait on the sound card.
//
// The worker is joinable. A launcher restart can install a new audio backend
// while an old restore is still inside its 20 s window, and that old worker
// must not write the volume it captured over the new session.
namespace {

struct SavedVolumeRestore
{
    std::mutex mutex;
    std::condition_variable wake;
    bool stop = false;
    std::thread worker;

    // Hosts that never call deinit_audio (the emulator does not) would reach
    // static destruction with a joinable thread, which terminates the process.
    // The audio callback lists live in this file and outlive this object, so
    // joining here is safe.
    ~SavedVolumeRestore()
    {
        try {
            join();
        } catch (...) {
        }
    }

    void request_stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        wake.notify_one();
    }

    void join()
    {
        request_stop();
        if (worker.joinable())
            worker.join();
    }

    void start(int volume)
    {
        join();
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = false;
        }
        worker = std::thread([this, volume] { run(volume); });
    }

    bool wait_for_next_pass()
    {
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait_for(lock, std::chrono::milliseconds(500), [this] { return stop; });
        return stop;
    }

    void run(int volume)
    {
        using clock = std::chrono::steady_clock;
        const auto deadline = clock::now() + std::chrono::seconds(20);
        const auto settle = std::chrono::seconds(2);
        Cp0VolumeRestoreStep state;
        state.target = volume;

        while (clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stop)
                    return;
            }

            std::string sink;
            cp0::signal::invoke_noexcept([&] {
                cp0_signal_audio_api({"DefaultSink"}, [&](int code, std::string data) {
                    if (code == 0)
                        sink = std::move(data);
                });
            });

            const int saved = config_get_int("volume", state.target);
            int current = -1;
            if (Cp0AudioMixerControl::is_hardware_sink_name(sink.c_str())) {
                cp0::signal::invoke_noexcept([&] {
                    cp0_signal_audio_api({"VolumeRead"}, [&](int code, std::string data) {
                        int parsed = 0;
                        if (code == 0 && cp0_integer_codec::parse_int(data, parsed))
                            current = parsed;
                    });
                });
            }

            state = cp0_volume_restore_step(state, sink.c_str(), saved, current,
                                             clock::now(), settle);
            if (state.action == Cp0VolumeRestoreStep::Action::Done)
                return;
            if (state.action == Cp0VolumeRestoreStep::Action::Write) {
                const std::string written = std::to_string(state.target);
                cp0::signal::invoke_noexcept([&] {
                    cp0_signal_audio_api({"VolumeWrite", written}, nullptr);
                });
            }
            if (wait_for_next_pass())
                return;
        }
    }
};

SavedVolumeRestore g_saved_volume_restore;

} // namespace

static void saved_volume_restore(int val)
{
    g_saved_volume_restore.start(val);
}

extern "C" void cp0_saved_volume_restore_stop(void) noexcept
{
    try {
        g_saved_volume_restore.join();
    } catch (...) {
    }
}

static void saved_gpio_write(const char *name)
{
    const int value = config_get_int(name, -1);
    if (value != 0 && value != 1)
        return;
    cp0::signal::invoke_noexcept([&] {
        cp0_signal_settings_api({"GpioSet", name, std::to_string(value)}, nullptr);
    });
}

extern "C" void init_lvgl_saved_settings()
{
    int saved_bright = config_get_int("brightness", -1);
    if (saved_bright > 0)
        cp0_backlight_write(saved_bright);

    int saved_vol = config_get_int("volume", -1);
    if (saved_vol >= 0)
        saved_volume_restore(saved_vol);

    saved_gpio_write("extport_usb");
    saved_gpio_write("extport_5vout");
}
