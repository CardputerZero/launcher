#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <iterator>
#include <list>
#include <sstream>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>

namespace {
class Cp0Filesystem {
public:
    void api_call(const std::list<std::string> &arg, std::function<void(int, std::string)> callback)
    {
        auto report = [&](int code, const std::string &data) {
            if (callback) callback(code, data);
        };

        if (arg.empty()) {
            report(-1, "missing command");
            return;
        }

        const std::string &cmd = arg.front();
        auto value_arg = [&]() -> std::string {
            return arg.size() >= 2 ? *std::next(arg.begin()) : std::string();
        };

        if (cmd == "Path") {
            report(0, resolve_path(value_arg()));
        } else if (cmd == "DirList") {
            std::string data;
            report(encode_dir_entries(value_arg().c_str(), data), data);
        } else if (cmd == "WatchStart") {
            cp0_watcher_t watcher = watch_start(value_arg().c_str());
            report(watcher ? 0 : -1, std::to_string(reinterpret_cast<uintptr_t>(watcher)));
        } else if (cmd == "WatchPoll") {
            auto *watcher = reinterpret_cast<void *>(parse_uintptr(value_arg()));
            report(0, std::to_string(watch_poll(watcher)));
        } else if (cmd == "WatchStop") {
            auto *watcher = reinterpret_cast<void *>(parse_uintptr(value_arg()));
            watch_stop(watcher);
            report(0, "");
        } else {
            report(-2, "unknown command: " + cmd);
        }
    }

    std::string path(std::string file)
    {
        int code = -1;
        std::string data;
        invoke({"Path", std::move(file)}, code, data);
        return code == 0 ? data : std::string();
    }

    int dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count)
    {
        int code = -1;
        std::string data;
        invoke({"DirList", path ? path : ""}, code, data);
        if (code != 0) return code;
        return decode_dir_entries(data, entries, max_entries, out_count);
    }

    cp0_watcher_t dir_watch_start(const char *path)
    {
        int code = -1;
        std::string data;
        invoke({"WatchStart", path ? path : ""}, code, data);
        return code == 0 ? reinterpret_cast<cp0_watcher_t>(parse_uintptr(data)) : nullptr;
    }

    int dir_watch_poll(cp0_watcher_t watcher)
    {
        int code = -1;
        std::string data;
        invoke({"WatchPoll", std::to_string(reinterpret_cast<uintptr_t>(watcher))}, code, data);
        return code == 0 ? std::atoi(data.c_str()) : code;
    }

    void dir_watch_stop(cp0_watcher_t watcher)
    {
        int code = -1;
        std::string data;
        invoke({"WatchStop", std::to_string(reinterpret_cast<uintptr_t>(watcher))}, code, data);
    }

private:
    struct Watcher {
        int inotify_fd;
        int watch_fd;
    };

    static constexpr const char *kAppRoot = "/usr/share/APPLaunch";
    static constexpr const char *kLvglFsRoot = "A:/";

    void invoke(std::list<std::string> arg, int &code, std::string &data)
    {
        api_call(arg, [&](int c, std::string d) {
            code = c;
            data = std::move(d);
        });
    }

    static uintptr_t parse_uintptr(const std::string &value)
    {
        return static_cast<uintptr_t>(std::strtoull(value.c_str(), nullptr, 0));
    }

    static std::string lower_ext(const std::string &file)
    {
        const auto dot = file.find_last_of('.');
        if (dot == std::string::npos) return "";

        std::string ext = file.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return ext;
    }

    static bool is_image_ext(const std::string &ext)
    {
        return ext == "png" || ext == "gif" || ext == "jpg" || ext == "jpeg" || ext == "svg";
    }

    static bool is_audio_ext(const std::string &ext)
    {
        return ext == "wav" || ext == "mp3" || ext == "ogg";
    }

    static bool is_font_ext(const std::string &ext)
    {
        return ext == "ttf" || ext == "otf";
    }

    static bool has_lvgl_drive(const std::string &file)
    {
        return file.size() >= 2 && std::isalpha(static_cast<unsigned char>(file[0])) && file[1] == ':';
    }

    static bool starts_with(const std::string &value, const char *prefix)
    {
        const std::string prefix_str(prefix);
        return value.compare(0, prefix_str.size(), prefix_str) == 0;
    }

    static std::string strip_app_root_prefix(const std::string &file)
    {
        const std::string app_root_prefix = std::string(kAppRoot) + "/";
        if (starts_with(file, app_root_prefix.c_str())) return file.substr(app_root_prefix.size());
        if (starts_with(file, "APPLaunch/")) return file.substr(std::strlen("APPLaunch/"));
        return file;
    }

#ifdef HAL_PLATFORM_SDL
    static bool path_exists(const std::string &path)
    {
        return access(path.c_str(), F_OK) == 0;
    }

    static const std::string &sdl_app_root()
    {
        static const std::string root = []() {
            const char *candidates[] = {
                "APPLaunch",
                "dist/APPLaunch",
                "../APPLaunch",
                "../dist/APPLaunch",
                "projects/APPLaunch/APPLaunch",
                "projects/APPLaunch/dist/APPLaunch",
            };
            for (const char *candidate : candidates) {
                if (path_exists(std::string(candidate) + "/share/images")) return std::string(candidate);
            }
            return std::string("APPLaunch");
        }();
        return root;
    }

    static std::string sdl_resource_path(const char *dir, const std::string &file)
    {
        std::string rel = strip_app_root_prefix(file);
        const std::string dir_prefix = std::string(dir) + "/";
        if (starts_with(rel, dir_prefix.c_str())) return sdl_app_root() + "/" + rel;
        if (!rel.empty() && rel.front() == '/') return rel;
        return sdl_app_root() + "/" + dir_prefix + rel;
    }
#endif

    static std::string lvgl_root_path(std::string rel)
    {
        while (!rel.empty() && rel.front() == '/') {
            rel.erase(rel.begin());
        }
        return std::string(kLvglFsRoot) + rel;
    }

    static std::string resolve_lvgl_image_path(const std::string &file)
    {
#ifdef HAL_PLATFORM_SDL
        return sdl_resource_path("share/images", file);
#else
        if (has_lvgl_drive(file)) return file;

        const std::string rel = strip_app_root_prefix(file);

        if (!rel.empty() && rel.front() == '/') return rel;
        if (starts_with(rel, "share/images/")) return lvgl_root_path(rel);

        return lvgl_root_path("share/images/" + rel);
#endif
    }

    static std::string resolve_path(const std::string &file)
    {
        if (file.empty()) return "";

#ifdef HAL_PLATFORM_SDL
        if (file == "applications") return sdl_app_root() + "/applications";
        if (file == "lock_file") return "/tmp/M5CardputerZero-APPLaunch_fcntl.lock";
        if (file == "keyboard_device") return "";
        if (file == "keyboard_map") return "";
        if (file == "store_sync_cmd") return std::string("python ") + sdl_app_root() + "/bin/store_cache_sync.py";
#else
        if (file == "applications") return std::string(kAppRoot) + "/applications";
        if (file == "lock_file") return "/tmp/M5CardputerZero-APPLaunch_fcntl.lock";
        if (file == "keyboard_device") return "/dev/input/by-path/platform-3f804000.i2c-event";
        if (file == "keyboard_map") return "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map";
        if (file == "store_sync_cmd") return std::string("python ") + kAppRoot + "/bin/store_cache_sync.py";
#endif

        const std::string ext = lower_ext(file);
        if (is_image_ext(ext)) return resolve_lvgl_image_path(file);
#ifdef HAL_PLATFORM_SDL
        if (is_audio_ext(ext)) return sdl_resource_path("share/audio", file);
        if (is_font_ext(ext)) return sdl_resource_path("share/font", file);
#else
        if (is_audio_ext(ext)) return std::string(kAppRoot) + "/share/audio/" + file;
        if (is_font_ext(ext)) return std::string(kAppRoot) + "/share/font/" + file;
#endif

        return file;
    }

    static int list_dir(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count)
    {
        if (out_count) *out_count = 0;
        if (!path || !out_count) return -1;
        if (!entries || max_entries <= 0) return 0;

        DIR *dir = opendir(path);
        if (!dir) return -1;

        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_name[0] == '.') continue;
            if (*out_count >= max_entries) break;
            std::strncpy(entries[*out_count].name, ent->d_name, sizeof(entries[*out_count].name) - 1);
            entries[*out_count].name[sizeof(entries[*out_count].name) - 1] = '\0';
            entries[*out_count].is_dir = (ent->d_type == DT_DIR) ? 1 : 0;
            (*out_count)++;
        }

        closedir(dir);
        return 0;
    }

    static int encode_dir_entries(const char *path, std::string &data)
    {
        cp0_dirent_t entries[512];
        int count = 0;
        if (list_dir(path, entries, 512, &count) != 0) return -1;

        std::ostringstream out;
        for (int i = 0; i < count; ++i) {
            out << (entries[i].is_dir ? 'D' : 'F') << '\t' << entries[i].name << '\n';
        }
        data = out.str();
        return 0;
    }

    static int decode_dir_entries(const std::string &data, cp0_dirent_t *entries, int max_entries, int *out_count)
    {
        if (out_count) *out_count = 0;
        if (!entries || max_entries <= 0 || !out_count) return 0;

        size_t start = 0;
        while (start < data.size() && *out_count < max_entries) {
            size_t end = data.find('\n', start);
            std::string line = data.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (line.size() >= 3 && line[1] == '\t') {
                entries[*out_count].is_dir = (line[0] == 'D') ? 1 : 0;
                std::strncpy(entries[*out_count].name, line.c_str() + 2, sizeof(entries[*out_count].name) - 1);
                entries[*out_count].name[sizeof(entries[*out_count].name) - 1] = '\0';
                (*out_count)++;
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return 0;
    }

    static cp0_watcher_t watch_start(const char *path)
    {
        if (!path) return nullptr;

        int fd = inotify_init1(IN_NONBLOCK);
        if (fd < 0) return nullptr;

        int wd = inotify_add_watch(fd, path, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
        if (wd < 0) {
            close(fd);
            return nullptr;
        }

        auto *watcher = static_cast<Watcher *>(std::malloc(sizeof(Watcher)));
        if (!watcher) {
            close(fd);
            return nullptr;
        }
        watcher->inotify_fd = fd;
        watcher->watch_fd = wd;
        return watcher;
    }

    static int watch_poll(cp0_watcher_t watcher)
    {
        if (!watcher) return -1;

        auto *w = static_cast<Watcher *>(watcher);
        char buf[1024] __attribute__((aligned(8)));
        ssize_t n = read(w->inotify_fd, buf, sizeof(buf));
        return (n > 0) ? 1 : 0;
    }

    static void watch_stop(cp0_watcher_t watcher)
    {
        if (!watcher) return;

        auto *w = static_cast<Watcher *>(watcher);
        if (w->watch_fd >= 0) inotify_rm_watch(w->inotify_fd, w->watch_fd);
        close(w->inotify_fd);
        std::free(w);
    }
};

Cp0Filesystem &filesystem()
{
    static Cp0Filesystem instance;
    return instance;
}
} // namespace

std::string cp0_file_path(std::string file)
{
    return filesystem().path(std::move(file));
}

extern "C" const char *cp0_file_path_c(const char *file)
{
    static thread_local std::unordered_map<std::string, std::string> paths;
    std::string key = file ? std::string(file) : std::string();
    auto it = paths.find(key);
    if (it == paths.end()) it = paths.emplace(key, cp0_file_path(key)).first;
    return it->second.c_str();
}

extern "C" int cp0_dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count)
{
    return filesystem().dir_list(path, entries, max_entries, out_count);
}

extern "C" cp0_watcher_t cp0_dir_watch_start(const char *path)
{
    return filesystem().dir_watch_start(path);
}

extern "C" int cp0_dir_watch_poll(cp0_watcher_t watcher)
{
    return filesystem().dir_watch_poll(watcher);
}

extern "C" void cp0_dir_watch_stop(cp0_watcher_t watcher)
{
    filesystem().dir_watch_stop(watcher);
}

extern "C" void init_filesystem(void)
{
    cp0_signal_filesystem_api.append([](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        filesystem().api_call(arg, std::move(callback));
    });
}
