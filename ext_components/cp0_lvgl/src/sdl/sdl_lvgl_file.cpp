#include "cp0_lvgl_app.h"
#include "cp0_lvgl_file.hpp"

#include <algorithm>
#include <cctype>
#include <limits.h>
#include <string>
#include <unistd.h>

namespace {
std::string get_app_root_path()
{
    char exe_path[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) {
        return "APPLaunch";
    }

    exe_path[len] = '\0';
    std::string path(exe_path);
    size_t slash = path.find_last_of('/');
    std::string exe_dir = (slash == std::string::npos) ? "." : path.substr(0, slash);

    return exe_dir + "/APPLaunch";
}

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

    const std::string root_path = get_app_root_path();
    if (file == "applications") {
        return root_path + "/applications";
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
        return "python " + root_path + "/bin/store_cache_sync.py";
    }

    const std::string ext = lower_ext(file);
    if (is_image_ext(ext)) {
        return "share/images/" + file;
    }
    if (is_audio_ext(ext)) {
        return root_path + "/share/audio/" + file;
    }
    if (is_font_ext(ext)) {
        return root_path + "/share/font/" + file;
    }

    return file;
}

extern "C" const char *cp0_file_path_c(const char *file)
{
    static thread_local std::string path;
    path = cp0_file_path(file ? std::string(file) : std::string());
    return path.c_str();
}
