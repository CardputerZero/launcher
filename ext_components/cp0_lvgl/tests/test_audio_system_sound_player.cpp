/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "cp0_audio_system_sound_player.hpp"

#include <cassert>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

std::string cp0_file_path(std::string file)
{
    return "/path/that/does/not/exist/" + file;
}

int main()
{
    Cp0SystemSoundPlayer player;
    assert(player.sound_count() == 3);
    assert(player.contains("Ding2.wav"));
    assert(!player.play_index(player.sound_count()));

    player.set_enabled(false);
    assert(!player.enabled());
    assert(!player.play_index(0));

    player.set_enabled(true);
    assert(player.enabled());

    std::promise<bool> result;
    auto future = result.get_future();
    std::thread::id callback_thread;
    const std::thread::id caller_thread = std::this_thread::get_id();
    const auto started = std::chrono::steady_clock::now();
    assert(player.play_index(0, [&result, &callback_thread](bool played) {
        callback_thread = std::this_thread::get_id();
        result.set_value(played);
    }));
    assert(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(50));
    assert(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    assert(!future.get());
    assert(callback_thread != caller_thread);

    assert(player.reload({"startup.wav", "left.wav", "right.wav"}) == 0);
    assert(player.contains("startup.wav"));
    assert(player.contains("left.wav"));
    assert(player.contains("right.wav"));
    assert(!player.contains("Ding2.wav"));

    // Extra sounds append after the platform slots, so reload()ing the platform
    // names later cannot displace them and the indexed slots keep their order.
    assert(player.add_named({"lock.mp3", "blocked.mp3", "select.mp3", "unlock.mp3"}) == 0);
    assert(player.sound_count() == 7);
    assert(player.contains("lock.mp3") && player.contains("unlock.mp3"));
    assert(player.contains("startup.wav") && player.contains("right.wav"));
    assert(!player.contains(""));

    // Registering the same name twice is a no-op, so a module that registers on
    // every init cannot grow the list without bound.
    assert(player.add_named({"lock.mp3", "unlock.mp3"}) == 0);
    assert(player.sound_count() == 7);
    assert(player.add_named({}) == 0);
    assert(player.sound_count() == 7);

    // Named playback is called on the UI thread; it must enqueue without
    // recursively locking the player's mutex or waiting for audio playback.
    auto named = std::async(std::launch::async, [&player] {
        return player.play_named("lock.mp3");
    });
    assert(named.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    assert(named.get());
    assert(!player.play_named(""));
    assert(!player.play_named("unknown.mp3"));
    player.set_enabled(false);
    assert(!player.play_named("lock.mp3"));
}
