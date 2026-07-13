#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <fstream>
#include <iterator>
#include <list>
#include <limits>
#include <new>
#include <sstream>
#include <string>
#include <sys/stat.h>
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
        } else if (cmd == "DirListDetail") {
            std::string data;
            report(encode_dir_entries_detail(value_arg().c_str(), data), data);
        } else if (cmd == "Exists") {
            report(0, access(value_arg().c_str(), R_OK) == 0 ? "1" : "0");
        } else if (cmd == "ReadFile") {
            const auto max_it = arg.size() >= 3 ? std::next(arg.begin(), 2) : arg.end();
            const size_t max_bytes = max_it == arg.end() ? std::numeric_limits<size_t>::max()
                : static_cast<size_t>(std::strtoull(max_it->c_str(), nullptr, 10));
            std::string data;
            report(read_file_limited(value_arg(), max_bytes, data), data);
        } else if (cmd == "EnsureDirForUser") {
            const std::string path = value_arg();
            int ret = mkdir(path.c_str(), 0755);
            struct stat st{};
            if ((ret != 0 && errno != EEXIST) || stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                report(-1, "");
            } else {
                if (getuid() == 0) {
                    uid_t uid = static_cast<uid_t>(std::atoi(std::getenv("SUDO_UID") ? std::getenv("SUDO_UID") : "1000"));
                    gid_t gid = static_cast<gid_t>(std::atoi(std::getenv("SUDO_GID") ? std::getenv("SUDO_GID") : "1000"));
                    if (chown(path.c_str(), uid, gid) != 0) {
                        report(-1, "");
                        return;
                    }
                }
                report(0, "");
            }
        } else if (cmd == "Touch") {
            const std::string path = value_arg();
            const auto slash = path.find_last_of('/');
            if (slash != std::string::npos && slash > 0) {
                const std::string parent = path.substr(0, slash);
                struct stat st{};
                if (mkdir(parent.c_str(), 0755) != 0 && errno != EEXIST) {
                    report(-1, "");
                    return;
                }
                if (stat(parent.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                    report(-1, "");
                    return;
                }
            }
            FILE *file = fopen(path.c_str(), "a");
            if (file) fclose(file);
            report(file ? 0 : -1, "");
        } else if (cmd == "Remove") {
            report(std::remove(value_arg().c_str()) == 0 ? 0 : -1, "");
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

    static std::string lvgl_root_path(std::string rel)
    {
        while (!rel.empty() && rel.front() == '/') {
            rel.erase(rel.begin());
        }
        return std::string(kLvglFsRoot) + rel;
    }

    static std::string resolve_lvgl_image_path(const std::string &file)
    {
        if (has_lvgl_drive(file)) return file;

        const std::string rel = strip_app_root_prefix(file);

        if (!rel.empty() && rel.front() == '/') return rel;
        if (starts_with(rel, "share/images/")) return lvgl_root_path(rel);

        return lvgl_root_path("share/images/" + rel);
    }

    static std::string resolve_path(const std::string &file)
    {
        if (file.empty()) return "";

        if (file == "applications") return std::string(kAppRoot) + "/applications";
        if (file == "appstore_exec") return std::string(kAppRoot) + "/bin/M5CardputerZero-AppStore";
        if (file == "calculator_exec") return std::string(kAppRoot) + "/bin/M5CardputerZero-Calculator";
        if (file == "adb_helper") return std::string(kAppRoot) + "/adb/cardputer-adb";
        if (file == "launcher_settings") return "/var/lib/applaunch/settings";
        if (file == "oobe_marker") return "/var/lib/applaunch/run-oobe";
        if (file == "home_dir") {
            const char *home = std::getenv("HOME");
            return home && home[0] ? home : "/tmp";
        }
        if (file == "lock_file") return "/tmp/M5CardputerZero-APPLaunch_fcntl.lock";
        if (file == "keyboard_device") return "/dev/input/by-path/platform-3f804000.i2c-event";
        if (file == "keyboard_map") return "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map";
        if (file == "store_sync_cmd") return std::string("python ") + kAppRoot + "/bin/store_cache_sync.py";

        const std::string ext = lower_ext(file);
        if (is_image_ext(ext)) return resolve_lvgl_image_path(file);
        if (is_audio_ext(ext)) return std::string(kAppRoot) + "/share/audio/" + file;
        if (is_font_ext(ext)) return std::string(kAppRoot) + "/share/font/" + file;

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
            if (is_dot_entry(ent->d_name)) continue;
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
        DIR *dir = opendir(path);
        if (!dir) return -errno;
        try {
            std::ostringstream out;
            struct dirent *ent;
            while ((ent = readdir(dir)) != nullptr) {
                if (is_dot_entry(ent->d_name)) continue;
                bool is_dir = ent->d_type == DT_DIR;
                if (ent->d_type == DT_UNKNOWN) {
                    const std::string full = std::string(path) + (std::string(path) == "/" ? "" : "/") + ent->d_name;
                    struct stat st{};
                    is_dir = stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
                }
                out << (is_dir ? 'D' : 'F') << '\t' << encode_field(ent->d_name) << '\n';
            }
            data = out.str();
        } catch (const std::bad_alloc &) {
            closedir(dir);
            return -ENOMEM;
        }
        closedir(dir);
        return 0;
    }

    static int read_file_limited(const std::string &path, size_t max_bytes, std::string &data)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return errno ? -errno : -EIO;
        const std::streamoff length = file.tellg();
        if (length < 0) return -EIO;
        if (static_cast<uintmax_t>(length) > max_bytes) return -EFBIG;
        try {
            data.resize(static_cast<size_t>(length));
        } catch (const std::bad_alloc &) {
            return -ENOMEM;
        }
        file.seekg(0);
        if (length > 0 && !file.read(&data[0], length)) {
            data.clear();
            return -EIO;
        }
        return 0;
    }

    static int encode_dir_entries_detail(const char *path, std::string &data)
    {
        DIR *dir = opendir(path);
        if (!dir) return -1;
        try {
            std::ostringstream out;
            struct dirent *ent;
            while ((ent = readdir(dir)) != nullptr) {
                if (is_dot_entry(ent->d_name)) continue;
                const std::string full = std::string(path) + (std::string(path) == "/" ? "" : "/") + ent->d_name;
                struct stat st;
                if (stat(full.c_str(), &st) != 0) continue;
                out << (S_ISDIR(st.st_mode) ? 'D' : 'F') << '\t' << static_cast<long long>(st.st_size)
                    << '\t' << encode_field(ent->d_name) << '\n';
            }
            data = out.str();
        } catch (const std::bad_alloc &) {
            closedir(dir);
            return -ENOMEM;
        }
        closedir(dir);
        return 0;
    }

    static bool is_dot_entry(const char *name)
    {
        return name && name[0] == '.' &&
               (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
    }

    static std::string encode_field(const char *value)
    {
        static constexpr char hex[] = "0123456789ABCDEF";
        std::string encoded;
        for (const unsigned char c : std::string(value ? value : "")) {
            if (c == '%' || c == '\t' || c == '\n' || c == '\r') {
                encoded.push_back('%');
                encoded.push_back(hex[c >> 4]);
                encoded.push_back(hex[c & 0x0f]);
            } else {
                encoded.push_back(static_cast<char>(c));
            }
        }
        return encoded;
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
                std::string name;
                if (!decode_field(line.substr(2), name)) {
                    if (end == std::string::npos) break;
                    start = end + 1;
                    continue;
                }
                std::strncpy(entries[*out_count].name, name.c_str(), sizeof(entries[*out_count].name) - 1);
                entries[*out_count].name[sizeof(entries[*out_count].name) - 1] = '\0';
                (*out_count)++;
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return 0;
    }

    static bool decode_field(const std::string &encoded, std::string &decoded)
    {
        auto hex_value = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        decoded.clear();
        for (size_t i = 0; i < encoded.size(); ++i) {
            if (encoded[i] != '%') { decoded.push_back(encoded[i]); continue; }
            if (i + 2 >= encoded.size()) return false;
            const int high = hex_value(encoded[i + 1]);
            const int low = hex_value(encoded[i + 2]);
            if (high < 0 || low < 0) return false;
            decoded.push_back(static_cast<char>((high << 4) | low));
            i += 2;
        }
        return true;
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
