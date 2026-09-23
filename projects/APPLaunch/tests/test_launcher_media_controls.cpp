/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../main/ui/launcher_media_controls.h"

#include "hal_lvgl_bsp.h"

#include <cassert>
#include <functional>
#include <iterator>
#include <list>
#include <string>
#include <vector>

eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_audio_api;
eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_config_api;
eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_settings_api;

namespace {

int sink_volume = 63;
int configured_volume = 80;
int backlight_maximum = 255;
int backlight_value = 102;
int configured_brightness = 102;
bool brightness_config_missing = false;
bool backlight_write_failed = false;
int config_writes = 0;
bool sink_muted = true;
int mute_toggles = 0;
std::vector<std::string> audio_commands;

void reply(const std::function<void(int, std::string)> &callback,
           int code, const std::string &data)
{
    if (callback)
        callback(code, data);
}

} // namespace

int main()
{
    cp0_signal_audio_api.append(
        [](std::list<std::string> arguments,
           std::function<void(int, std::string)> callback) {
            assert(!arguments.empty());
            const std::string command = arguments.front();
            audio_commands.push_back(command);

            if (command == "MuteRead") {
                reply(callback, 0, sink_muted ? "1" : "0");
            } else if (command == "MuteToggle") {
                sink_muted = !sink_muted;
                ++mute_toggles;
                reply(callback, 0, sink_muted ? "1" : "0");
            } else if (command == "VolumeRead") {
                reply(callback, 0, std::to_string(sink_volume));
            } else if (command == "VolumeWrite") {
                assert(arguments.size() == 2);
                sink_volume = std::stoi(*std::next(arguments.begin()));
                reply(callback, 0, std::to_string(sink_volume));
            } else {
                assert(false);
            }
        });

    cp0_signal_config_api.append(
        [](std::list<std::string> arguments,
           std::function<void(int, std::string)> callback) {
            assert(!arguments.empty());
            const std::string command = arguments.front();
            if (command == "GetInt") {
                const std::string key = *std::next(arguments.begin());
                if (key == "brightness" && brightness_config_missing) {
                    reply(callback, 0, *std::next(arguments.begin(), 2));
                    return;
                }
                reply(callback, 0, std::to_string(
                    key == "brightness" ? configured_brightness : configured_volume));
            } else if (command == "SetInt") {
                ++config_writes;
                assert(arguments.size() == 3);
                const std::string key = *std::next(arguments.begin());
                const int value = std::stoi(*std::next(arguments.begin(), 2));
                if (key == "brightness")
                    configured_brightness = value;
                else
                    configured_volume = value;
                reply(callback, 0, "");
            } else if (command == "Save") {
                reply(callback, 0, "");
            } else {
                assert(false);
            }
        });

    cp0_signal_settings_api.append(
        [](std::list<std::string> arguments,
           std::function<void(int, std::string)> callback) {
            assert(!arguments.empty());
            const std::string command = arguments.front();
            if (command == "BacklightMax") {
                reply(callback, 0, std::to_string(backlight_maximum));
            } else if (command == "BacklightRead") {
                reply(callback, 0, std::to_string(backlight_value));
            } else if (command == "BacklightWrite") {
                assert(arguments.size() == 2);
                if (backlight_write_failed) {
                    reply(callback, -1, "write failed");
                    return;
                }
                backlight_value = std::stoi(*std::next(arguments.begin()));
                reply(callback, 0, std::to_string(backlight_value));
            } else {
                assert(false);
            }
        });

    assert(launcher_media_controls::VOLUME_STEP_PERCENT == 10);
    assert(launcher_media_controls::adjust_volume(-10) == 50);
    assert(!sink_muted);
    assert(sink_volume == 50);
    assert(configured_volume == 50);
    assert(mute_toggles == 1);
    assert(audio_commands.size() >= 4);
    assert(audio_commands[0] == "MuteRead");
    assert(audio_commands[1] == "MuteToggle");
    assert(audio_commands[2] == "VolumeRead");
    assert(audio_commands[3] == "VolumeWrite");

    sink_muted = true;
    sink_volume = 63;
    assert(launcher_media_controls::adjust_volume(10) == 70);
    assert(!sink_muted);
    assert(sink_volume == 70);
    assert(configured_volume == 70);
    assert(mute_toggles == 2);

    sink_volume = 71;
    assert(launcher_media_controls::adjust_volume(10) == 80);
    assert(!sink_muted);
    assert(sink_volume == 80);
    assert(configured_volume == 80);
    assert(mute_toggles == 2);

    sink_volume = 82;
    assert(launcher_media_controls::adjust_volume(-10) == 70);
    assert(sink_volume == 70);
    assert(configured_volume == 70);
    assert(mute_toggles == 2);

    assert(launcher_media_controls::adjust_brightness(-10) == 30);
    assert(backlight_value == 76);
    assert(configured_brightness == 76);

    // Simulate Settings changing the shared hardware/config value after the shortcut cache exists.
    backlight_value = 178;
    configured_brightness = 178;
    assert(launcher_media_controls::adjust_brightness(-10) == 60);
    assert(backlight_value == 153);
    assert(configured_brightness == 153);
    assert(launcher_media_controls::adjust_brightness(10) == 70);
    assert(backlight_value == 178);

    backlight_value = 25;
    configured_brightness = 25;
    assert(launcher_media_controls::adjust_brightness(-10) == 10);
    assert(backlight_value == 25);
    assert(backlight_value > 0);

    backlight_value = 255;
    configured_brightness = 255;
    assert(launcher_media_controls::adjust_brightness(10) == 100);
    assert(backlight_value == 255);

    // Screen-off suspends the panel without touching the persisted brightness.
    backlight_value = 178;
    configured_brightness = 178;
    const int suspended = launcher_media_controls::suspend_backlight();
    assert(suspended == 178);
    assert(backlight_value == 0);
    assert(configured_brightness == 178);

    launcher_media_controls::restore_backlight(suspended);
    assert(backlight_value == 178);
    assert(configured_brightness == 178);

    // Nothing captured means there is nothing to restore.
    launcher_media_controls::restore_backlight(0);
    launcher_media_controls::restore_backlight(-1);
    assert(backlight_value == 178);

    // A zero reading carries no restore information, so the persisted step is
    // used instead of leaving the panel dark after wake-up.
    backlight_value = 0;
    const int fallback = launcher_media_controls::suspend_backlight();
    assert(fallback > 0 && fallback <= backlight_maximum);
    assert(backlight_value == 0);
    launcher_media_controls::restore_backlight(fallback);
    assert(backlight_value == fallback);
    assert(configured_brightness == 178);

    // Restart after screen-off must restore the saved raw value, even when it
    // comes from a legacy brightness level outside the current 10% steps.
    const int previous_config_writes = config_writes;
    configured_brightness = 64;
    backlight_value = 0;
    assert(launcher_media_controls::restore_startup_backlight());
    assert(backlight_value == 64);
    assert(configured_brightness == 64);

    for (int saved : {0, -1, 999}) {
        configured_brightness = saved;
        backlight_value = 0;
        assert(launcher_media_controls::restore_startup_backlight());
        assert(backlight_value == backlight_maximum);
        assert(configured_brightness == saved);
    }
    brightness_config_missing = true;
    backlight_value = 0;
    assert(launcher_media_controls::restore_startup_backlight());
    assert(backlight_value == backlight_maximum);
    brightness_config_missing = false;

    backlight_write_failed = true;
    backlight_value = 0;
    assert(!launcher_media_controls::restore_startup_backlight());
    assert(backlight_value == 0);
    backlight_write_failed = false;
    assert(config_writes == previous_config_writes);

    return 0;
}
