/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../main/ui/model/lockscreen_wallpaper.hpp"

#include <cassert>
#include <cstdlib>
#include <string>

namespace {

using lockscreen_wallpaper::Choice;
using lockscreen_wallpaper::ConfigStatus;

constexpr const char *kFallback = "A:/usr/share/APPLaunch/share/images/lockscreen.png";

bool always_readable(const std::string &)
{
    return true;
}

bool never_readable(const std::string &)
{
    return false;
}

} // namespace

int main()
{
    /* The override lives next to the launcher's own config, not in the shared
     * ~/.config/cardputerzero store. */
    assert(lockscreen_wallpaper::config_file_path("/home/pi") ==
           "/home/pi/.config/APPLaunch/config.json");
    assert(lockscreen_wallpaper::config_file_path("/home/pi/") ==
           "/home/pi/.config/APPLaunch/config.json");
    assert(lockscreen_wallpaper::config_file_path("").empty());

    std::string entry;
    assert(lockscreen_wallpaper::parse_entry("", entry) == ConfigStatus::Missing);
    assert(entry.empty());
    assert(lockscreen_wallpaper::parse_entry("{", entry) == ConfigStatus::Invalid);
    assert(lockscreen_wallpaper::parse_entry("not json", entry) == ConfigStatus::Invalid);
    assert(lockscreen_wallpaper::parse_entry("[]", entry) == ConfigStatus::Invalid);
    assert(lockscreen_wallpaper::parse_entry("{}", entry) == ConfigStatus::Missing);
    assert(lockscreen_wallpaper::parse_entry("{\"dark_time\": 30}", entry) ==
           ConfigStatus::Missing);

    /* The flat key is what the launcher reads; unrelated entries and other
     * value types in the same file are tolerated. */
    assert(lockscreen_wallpaper::parse_entry(
               "{\"dark_time\": 30, \"lockscreen_wallpaper\": \"/tmp/a.png\"}", entry) ==
           ConfigStatus::Found);
    assert(entry == "/tmp/a.png");

    /* A nested object maps to dotted keys, so the flat entry stays required. */
    assert(lockscreen_wallpaper::parse_entry(
               "{\"lockscreen_wallpaper\": {\"path\": \"/tmp/a.png\"}}", entry) ==
           ConfigStatus::Missing);
    /* null and blank values mean "not configured". */
    assert(lockscreen_wallpaper::parse_entry(
               "{\"lockscreen_wallpaper\": null}", entry) == ConfigStatus::Missing);
    assert(lockscreen_wallpaper::parse_entry(
               "{\"lockscreen_wallpaper\": \"   \"}", entry) == ConfigStatus::Missing);
    /* Escapes resolve to the real path; an escaped NUL is rejected. */
    assert(lockscreen_wallpaper::parse_entry(
               "{\"lockscreen_wallpaper\": \"/tmp/a\\\\b.png\"}", entry) ==
           ConfigStatus::Found);
    assert(entry == "/tmp/a\\b.png");
    assert(lockscreen_wallpaper::parse_entry(
               "{\"lockscreen_wallpaper\": \"\\u0000x\"}", entry) ==
           ConfigStatus::Missing);
    /* Non-string payloads are not usable as a path. */
    assert(lockscreen_wallpaper::parse_entry(
               "{\"lockscreen_wallpaper\": 42}", entry) == ConfigStatus::Found);
    assert(entry == "42");

    /* Path resolution: absolute, home-relative and config-relative entries. */
    assert(lockscreen_wallpaper::resolve_path("/tmp/a.png", "/home/pi") == "/tmp/a.png");
    assert(lockscreen_wallpaper::resolve_path("~/Pictures/a.png", "/home/pi") ==
           "/home/pi/Pictures/a.png");
    assert(lockscreen_wallpaper::resolve_path("~", "/home/pi") == "/home/pi");
    assert(lockscreen_wallpaper::resolve_path("wallpaper.png", "/home/pi") ==
           "/home/pi/.config/APPLaunch/wallpaper.png");
    assert(lockscreen_wallpaper::resolve_path("./img/../wallpaper.png", "/home/pi") ==
           "/home/pi/.config/APPLaunch/wallpaper.png");
    assert(lockscreen_wallpaper::resolve_path("../wallpaper.png", "/home/pi") ==
           "/home/pi/.config/wallpaper.png");
    assert(lockscreen_wallpaper::resolve_path("  /tmp/a.png  ", "/home/pi") ==
           "/tmp/a.png");
    assert(lockscreen_wallpaper::resolve_path("", "/home/pi").empty());
    assert(lockscreen_wallpaper::resolve_path("   ", "/home/pi").empty());
    assert(lockscreen_wallpaper::resolve_path(std::string("bad\0path", 8), "/home/pi")
               .empty());
    assert(lockscreen_wallpaper::resolve_path("relative.png", "").empty());

    /* No file or no entry: the packaged wallpaper, quietly. */
    Choice choice = lockscreen_wallpaper::choose("", "/home/pi", always_readable, kFallback);
    assert(!choice.overridden);
    assert(choice.path == kFallback);
    assert(choice.candidate.empty());
    assert(choice.status == ConfigStatus::Missing);

    /* A present, readable override wins. */
    choice = lockscreen_wallpaper::choose(
        "{\"lockscreen_wallpaper\": \"/tmp/a.png\"}", "/home/pi", always_readable,
        kFallback);
    assert(choice.overridden);
    assert(choice.path == "/tmp/a.png");
    assert(choice.candidate == "/tmp/a.png");

    /* Readability is injected; an unreadable override keeps the packaged
     * wallpaper and stays visible to the caller for logging. */
    choice = lockscreen_wallpaper::choose(
        "{\"lockscreen_wallpaper\": \"wall.png\"}", "/home/pi", never_readable, kFallback);
    assert(!choice.overridden);
    assert(choice.path == kFallback);
    assert(choice.candidate == "/home/pi/.config/APPLaunch/wall.png");

    choice = lockscreen_wallpaper::choose(
        "{\"lockscreen_wallpaper\": \"/tmp/a.png\"}", "/home/pi",
        [](const std::string &path) { return path == "/tmp/a.png"; }, kFallback);
    assert(choice.overridden && choice.path == "/tmp/a.png");

    /* Invalid JSON keeps the packaged wallpaper and reports why. */
    choice = lockscreen_wallpaper::choose("{", "/home/pi", always_readable, kFallback);
    assert(!choice.overridden);
    assert(choice.path == kFallback);
    assert(choice.status == ConfigStatus::Invalid);

    /* $HOME provides the location; without one the platform's /root fallback
     * applies (matching the config backend). */
    const char *saved_home = std::getenv("HOME");
    const std::string saved_home_value = saved_home ? saved_home : "";
    setenv("HOME", "/home/tester", 1);
    assert(lockscreen_wallpaper::home_directory() == "/home/tester");
    unsetenv("HOME");
    assert(lockscreen_wallpaper::home_directory() == "/root");
    if (saved_home) setenv("HOME", saved_home_value.c_str(), 1);

    return 0;
}
