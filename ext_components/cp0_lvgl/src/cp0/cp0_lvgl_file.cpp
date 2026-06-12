#include "cp0_lvgl_app.h"
#include "cp0_lvgl_file.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace {
constexpr const char *kAppRoot = "/usr/share/APPLaunch";

std::string lower_ext(const std::string &file)
{
    const auto dot = file.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }

    std::string ext = file.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

bool is_image_ext(const std::string &ext)
{
    return ext == "png" || ext == "gif" || ext == "jpg" || ext == "jpeg" || ext == "svg";
}

bool is_audio_ext(const std::string &ext)
{
    return ext == "wav" || ext == "mp3" || ext == "ogg";
}

bool is_font_ext(const std::string &ext)
{
    return ext == "ttf" || ext == "otf";
}

} // namespace

std::string cp0_file_path(std::string file)
{
    if (file.empty()) {
        return "";
    }

    if (file == "applications") {
        return std::string(kAppRoot) + "/applications";
    }
    if (file == "lock_file") {
        return "/tmp/M5CardputerZero-APPLaunch_fcntl.lock";
    }
    if (file == "keyboard_device") {
        return "/dev/input/by-path/platform-3f804000.i2c-event";
    }
    if (file == "keyboard_map") {
        return "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map";
    }
    if (file == "store_sync_cmd") {
        return std::string("python ") + kAppRoot + "/bin/store_cache_sync.py";
    }

    const std::string ext = lower_ext(file);
    if (is_image_ext(ext)) {
        return "share/images/" + file;
    }
    if (is_audio_ext(ext)) {
        return std::string(kAppRoot) + "/share/audio/" + file;
    }
    if (is_font_ext(ext)) {
        return std::string(kAppRoot) + "/share/font/" + file;
    }

    return file;
}

extern "C" const char *cp0_file_path_c(const char *file)
{
    static thread_local std::unordered_map<std::string, std::string> paths;
    std::string key = file ? std::string(file) : std::string();
    auto it = paths.find(key);
    if (it == paths.end()) {
        it = paths.emplace(key, cp0_file_path(key)).first;
    }
    return it->second.c_str();
}
