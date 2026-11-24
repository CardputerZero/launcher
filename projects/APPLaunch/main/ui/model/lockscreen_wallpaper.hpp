/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

/*
 * Per-user lock-screen wallpaper override.
 *
 * The launcher keeps its general settings in the shared
 * ~/.config/cardputerzero/config.json store.  The wallpaper override is a
 * launcher-specific drop-in file so it can be changed without touching that
 * shared contract:
 *
 *   ~/.config/APPLaunch/config.json
 *   { "lockscreen_wallpaper": "/home/pi/Pictures/aurora.png" }
 *
 * The entry is optional.  A missing file, invalid JSON, a missing key, an
 * unreadable path or an image that fails to decode all fall back to the
 * packaged share/images/lockscreen.png.
 */
namespace lockscreen_wallpaper {

/* Flat key inside ~/.config/APPLaunch/config.json. */
inline constexpr char kConfigKey[] = "lockscreen_wallpaper";

/* The override file is hand-written; anything bigger than this is a mistake. */
inline constexpr std::size_t kMaxConfigBytes = 16 * 1024;

/* The lock-screen panel is 320x150.  A wallpaper much larger than that costs
 * decode memory without adding detail, so refuse absurd photos instead of
 * risking an OOM on the device.  2048x2048 ARGB is the practical upper bound. */
inline constexpr uint64_t kMaxPixels = 2048ull * 2048ull;

enum class ConfigStatus {
    Missing, /* no override file (empty text) or no entry in it */
    Found,   /* entry present and non-empty */
    Invalid, /* readable text, but not the JSON object this file accepts */
};

struct Choice
{
    /* Path to decode; the fallback unless overridden is true. */
    std::string path;
    bool overridden = false;
    /* Resolved configured path when an entry was present, even if rejected. */
    std::string candidate;
    ConfigStatus status = ConfigStatus::Missing;
};

/* $HOME, or /root when the environment has none, mirroring the platform
 * config backend. */
std::string home_directory();

/* <home>/.config/APPLaunch/config.json, or empty when home is empty. */
std::string config_file_path(const std::string &home);

/* Parse the override file text and report the raw entry value. */
ConfigStatus parse_entry(const std::string &json_text, std::string &out_entry);

/* Turn the configured entry into a native absolute path.  Accepted forms:
 *   /absolute/path    used as-is
 *   ~/path or ~       relative to <home>
 *   relative/path     relative to <home>/.config/APPLaunch/
 * Returns an empty string for an empty or unusable entry. */
std::string resolve_path(const std::string &entry, const std::string &home);

/* Decide which wallpaper to load.  is_readable is injected so the decision
 * stays free of the platform file API. */
Choice choose(const std::string &json_text,
              const std::string &home,
              const std::function<bool(const std::string &)> &is_readable,
              const std::string &fallback_path);

} // namespace lockscreen_wallpaper
