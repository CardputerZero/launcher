#include "cp0_lvgl_app.h"
#include "cp0_lvgl_file.hpp"

#include <algorithm>
#include <cctype>
#include <limits.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
std::string dirname_of(const std::string &path)
{
    size_t slash = path.find_last_of('/');
    return (slash == std::string::npos) ? "." : path.substr(0, slash);
}

bool path_exists(const std::string &path)
{
    return access(path.c_str(), F_OK) == 0;
}

std::string get_exe_dir()
{
    char exe_path[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) {
        return ".";
    }

    exe_path[len] = '\0';
    return dirname_of(exe_path);
}

std::string get_cwd()
{
    char cwd[PATH_MAX] = {0};
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        return ".";
    }
    return cwd;
}

std::string normalize_path(const std::string &path)
{
    if (path.empty()) {
        return ".";
    }

    const bool absolute = path[0] == '/';
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part.empty() || part == ".") {
            // Skip.
        } else if (part == "..") {
            if (!parts.empty() && parts.back() != "..") {
                parts.pop_back();
            } else if (!absolute) {
                parts.push_back(part);
            }
        } else {
            parts.push_back(part);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    std::string out = absolute ? "/" : "";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out += "/";
        }
        out += parts[i];
    }
    return out.empty() ? (absolute ? "/" : ".") : out;
}

std::string make_absolute_path(const std::string &path)
{
    if (!path.empty() && path[0] == '/') {
        return normalize_path(path);
    }
    return normalize_path(get_cwd() + "/" + path);
}

std::vector<std::string> split_path(const std::string &path)
{
    std::vector<std::string> parts;
    size_t start = (path.size() > 0 && path[0] == '/') ? 1 : 0;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!part.empty()) {
            parts.push_back(part);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

std::string make_relative_to_cwd(const std::string &path)
{
    const std::string abs_path = make_absolute_path(path);
    const std::string abs_cwd = make_absolute_path(get_cwd());
    if (abs_path == abs_cwd) {
        return ".";
    }

    const auto path_parts = split_path(abs_path);
    const auto cwd_parts = split_path(abs_cwd);

    size_t common = 0;
    while (common < path_parts.size() && common < cwd_parts.size() && path_parts[common] == cwd_parts[common]) {
        ++common;
    }

    std::string rel;
    for (size_t i = common; i < cwd_parts.size(); ++i) {
        if (!rel.empty()) {
            rel += "/";
        }
        rel += "..";
    }
    for (size_t i = common; i < path_parts.size(); ++i) {
        if (!rel.empty()) {
            rel += "/";
        }
        rel += path_parts[i];
    }

    return rel.empty() ? "." : rel;
}

std::string get_app_root_path()
{
    const std::string exe_dir = get_exe_dir();
    const std::string cwd = get_cwd();
    const std::string exe_parent = dirname_of(exe_dir);

    std::vector<std::string> candidates = {
        exe_dir + "/APPLaunch",
        cwd + "/APPLaunch",
        exe_parent + "/APPLaunch",
    };

    for (const auto &candidate : candidates) {
        if (path_exists(candidate + "/share")) {
            return make_absolute_path(candidate);
        }
    }

    return make_absolute_path(exe_dir + "/APPLaunch");
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

bool is_absolute_path(const std::string &file)
{
    return !file.empty() && file[0] == '/';
}

bool starts_with(const std::string &value, const char *prefix)
{
    const std::string p(prefix);
    return value.compare(0, p.size(), p) == 0;
}

std::string app_relative_path(const std::string &root_path, const std::string &file, const char *dir)
{
    std::string absolute_path;
    if (starts_with(file, "APPLaunch/")) {
        absolute_path = dirname_of(root_path) + "/" + file;
    } else if (is_absolute_path(file)) {
        absolute_path = file;
    } else if (starts_with(file, dir)) {
        absolute_path = root_path + "/" + file;
    } else {
        absolute_path = root_path + "/" + dir + file;
    }

    return make_relative_to_cwd(absolute_path);
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
        return app_relative_path(root_path, file, "share/images/");
    }
    if (is_audio_ext(ext)) {
        return app_relative_path(root_path, file, "share/audio/");
    }
    if (is_font_ext(ext)) {
        return app_relative_path(root_path, file, "share/font/");
    }

    return file;
}

extern "C" const char *cp0_file_path_c(const char *file)
{
    static thread_local std::string path;
    path = cp0_file_path(file ? std::string(file) : std::string());
    return path.c_str();
}
