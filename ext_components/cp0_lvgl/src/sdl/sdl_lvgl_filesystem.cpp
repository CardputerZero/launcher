#include "cp0_lvgl_app.h"
#include "cp0_lvgl_filesystem.hpp"
#include "hal/hal_paths.h"
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
#include <unordered_map>
#include <utility>
#include <unistd.h>

static char s_data_dir[512] = ".";
static char s_applications_dir[512] = "./applications";
static char s_store_cache_dir[512] = "./store_cache";
static char s_lock_file[512] = "/tmp/M5CardputerZero-APPLaunch_fcntl.lock";
static char s_font_dir[512] = "./APPLaunch/share/font";
static char s_font_regular[512] = "./APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf";
static char s_font_mono[512] = "./APPLaunch/share/font/LiberationMono-Regular.ttf";
static char s_images_dir[512] = "./APPLaunch/share/images";
static char s_audio_dir[512] = "./APPLaunch/share/audio";
static char s_store_sync_cmd[512] = "python store_cache_sync.py";

extern "C" void hal_paths_init(const char *exe_dir)
{
    if (!exe_dir)
        exe_dir = ".";
    std::snprintf(s_data_dir, sizeof(s_data_dir), "%s", exe_dir);
    std::snprintf(s_applications_dir, sizeof(s_applications_dir), "%s/applications", exe_dir);
    std::snprintf(s_store_cache_dir, sizeof(s_store_cache_dir), "%s/store_cache", exe_dir);
    std::snprintf(s_images_dir, sizeof(s_images_dir), "%s/APPLaunch/share/images", exe_dir);
    std::snprintf(s_font_dir, sizeof(s_font_dir), "%s/APPLaunch/share/font", exe_dir);
    std::snprintf(s_audio_dir, sizeof(s_audio_dir), "%s/APPLaunch/share/audio", exe_dir);
    std::snprintf(s_font_regular, sizeof(s_font_regular),
                  "%s/APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf", exe_dir);
    std::snprintf(s_font_mono, sizeof(s_font_mono), "%s/APPLaunch/share/font/LiberationMono-Regular.ttf", exe_dir);
    std::snprintf(s_store_sync_cmd, sizeof(s_store_sync_cmd), "python %s/bin/store_cache_sync.py", exe_dir);
}

extern "C" const char *hal_path_data_dir(void) { return s_data_dir; }
extern "C" const char *hal_path_applications_dir(void) { return s_applications_dir; }
extern "C" const char *hal_path_store_cache_dir(void) { return s_store_cache_dir; }
extern "C" const char *hal_path_lock_file(void) { return s_lock_file; }
extern "C" const char *hal_path_font_dir(void) { return s_font_dir; }
extern "C" const char *hal_path_font_regular(void) { return s_font_regular; }
extern "C" const char *hal_path_font_mono(void) { return s_font_mono; }
extern "C" const char *hal_path_keyboard_device(void) { return nullptr; }
extern "C" const char *hal_path_keyboard_map(void) { return nullptr; }
extern "C" const char *hal_path_store_sync_cmd(void) { return s_store_sync_cmd; }
extern "C" const char *hal_path_images_dir(void) { return s_images_dir; }
extern "C" const char *hal_path_audio_dir(void) { return s_audio_dir; }

namespace {

static void copy_cstr(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0)
        return;
    if (!src)
        src = "";
    std::strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static bool starts_with(const std::string &value, const char *prefix)
{
    const std::string p(prefix ? prefix : "");
    return value.compare(0, p.size(), p) == 0;
}

static bool has_lvgl_drive(const std::string &file)
{
    return file.size() >= 2 && std::isalpha(static_cast<unsigned char>(file[0])) && file[1] == ':';
}

static std::string lower_ext(const std::string &file)
{
    const size_t dot = file.find_last_of('.');
    if (dot == std::string::npos)
        return "";
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

static std::string strip_app_root_prefix(const std::string &file)
{
    static constexpr const char *kAppRoot = "/usr/share/APPLaunch/";
    if (starts_with(file, kAppRoot))
        return file.substr(std::strlen(kAppRoot));
    if (starts_with(file, "APPLaunch/"))
        return file.substr(std::strlen("APPLaunch/"));
    return file;
}

static std::string join_path(const char *base, const std::string &rel)
{
    if (rel.empty())
        return base ? std::string(base) : std::string();
    if (rel.front() == '/' || has_lvgl_drive(rel))
        return rel;
    std::string out = base ? std::string(base) : std::string(".");
    if (!out.empty() && out.back() != '/')
        out.push_back('/');
    out += rel;
    return out;
}

static std::string resource_path(const char *base, const char *prefix, const std::string &file)
{
    std::string rel = strip_app_root_prefix(file);
    if (has_lvgl_drive(rel)) {
        rel = rel.substr(2);
        while (!rel.empty() && rel.front() == '/')
            rel.erase(rel.begin());
    }
    const std::string prefix_with_slash = std::string(prefix) + "/";
    if (starts_with(rel, prefix_with_slash.c_str()))
        rel = rel.substr(prefix_with_slash.size());
    return join_path(base, rel);
}

struct SdlWatcher {
    std::string path;
    time_t mtime = 0;
    off_t size = 0;
    nlink_t nlink = 0;
    bool valid = false;
};

class SdlFilesystem {
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    void api_call(arg_t arg, callback_t callback)
    {
        auto report = [&](int code, const std::string &data) {
            if (callback)
                callback(code, data);
        };

        if (arg.empty()) {
            report(-1, "missing command");
            return;
        }

        const std::string cmd = arg.front();
        const std::string value = arg.size() >= 2 ? *std::next(arg.begin()) : std::string();
        if (cmd == "Path") {
            report(0, resolve_path(value));
        } else if (cmd == "DirList") {
            std::string data;
            report(encode_dir_entries(value.c_str(), data), data);
        } else if (cmd == "DirListDetail") {
            std::string data;
            report(encode_dir_entries_detail(value.c_str(), data), data);
        } else if (cmd == "Exists") {
            report(0, access(value.c_str(), R_OK) == 0 ? "1" : "0");
        } else if (cmd == "ReadFile") {
            const auto max_it = arg.size() >= 3 ? std::next(arg.begin(), 2) : arg.end();
            const size_t max_bytes = max_it == arg.end() ? std::numeric_limits<size_t>::max()
                : static_cast<size_t>(std::strtoull(max_it->c_str(), nullptr, 10));
            std::string data;
            report(read_file_limited(value, max_bytes, data), data);
        } else if (cmd == "EnsureDirForUser") {
            int ret = mkdir(value.c_str(), 0755);
            struct stat st{};
            report((ret == 0 || errno == EEXIST) && stat(value.c_str(), &st) == 0 && S_ISDIR(st.st_mode) ? 0 : -1, "");
        } else if (cmd == "Touch") {
            const auto slash = value.find_last_of('/');
            if (slash != std::string::npos && slash > 0) {
                const std::string parent = value.substr(0, slash);
                struct stat st{};
                if ((mkdir(parent.c_str(), 0755) != 0 && errno != EEXIST) ||
                    stat(parent.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                    report(-1, "");
                    return;
                }
            }
            FILE *file = fopen(value.c_str(), "a");
            if (file) fclose(file);
            report(file ? 0 : -1, "");
        } else if (cmd == "Remove") {
            report(std::remove(value.c_str()) == 0 ? 0 : -1, "");
        } else if (cmd == "WatchStart") {
            cp0_watcher_t watcher = watch_start(value.c_str());
            report(watcher ? 0 : -1, std::to_string(reinterpret_cast<uintptr_t>(watcher)));
        } else if (cmd == "WatchPoll") {
            report(0, std::to_string(watch_poll(parse_handle(value))));
        } else if (cmd == "WatchStop") {
            watch_stop(parse_handle(value));
            report(0, "");
        } else {
            report(-2, "unknown command: " + cmd);
        }
    }

    std::string path(std::string file) { return resolve_path(file); }

    int dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count)
    {
        return list_dir(path, entries, max_entries, out_count);
    }

    cp0_watcher_t dir_watch_start(const char *path) { return watch_start(path); }
    int dir_watch_poll(cp0_watcher_t watcher) { return watch_poll(watcher); }
    void dir_watch_stop(cp0_watcher_t watcher) { watch_stop(watcher); }

private:
    static cp0_watcher_t parse_handle(const std::string &value)
    {
        return reinterpret_cast<cp0_watcher_t>(static_cast<uintptr_t>(std::strtoull(value.c_str(), nullptr, 0)));
    }

    static std::string resolve_path(const std::string &file)
    {
        if (file.empty())
            return "";
        if (file == "applications")
            return hal_path_applications_dir();
        if (file == "appstore_exec")
            return join_path(hal_path_data_dir(), "bin/M5CardputerZero-AppStore");
        if (file == "calculator_exec")
            return join_path(hal_path_data_dir(), "bin/M5CardputerZero-Calculator");
        if (file == "adb_helper")
            return join_path(hal_path_data_dir(), "adb/cardputer-adb");
        if (file == "launcher_settings")
            return join_path(hal_path_data_dir(), "settings");
        if (file == "oobe_marker")
            return join_path(hal_path_data_dir(), "run-oobe");
        if (file == "home_dir") {
            const char *home = std::getenv("HOME");
            return home && home[0] ? home : ".";
        }
        if (file == "lock_file")
            return hal_path_lock_file();
        if (file == "keyboard_device")
            return hal_path_keyboard_device() ? hal_path_keyboard_device() : "";
        if (file == "keyboard_map")
            return hal_path_keyboard_map() ? hal_path_keyboard_map() : "";
        if (file == "store_sync_cmd")
            return hal_path_store_sync_cmd();

        const std::string ext = lower_ext(file);
        if (is_image_ext(ext))
            return resource_path(hal_path_images_dir(), "share/images", file);
        if (is_audio_ext(ext))
            return resource_path(hal_path_audio_dir(), "share/audio", file);
        if (is_font_ext(ext))
            return resource_path(hal_path_font_dir(), "share/font", file);
        if (has_lvgl_drive(file)) {
            std::string rel = file.substr(2);
            while (!rel.empty() && rel.front() == '/')
                rel.erase(rel.begin());
            return rel;
        }
        return file;
    }

    static int list_dir(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count)
    {
        if (out_count)
            *out_count = 0;
        if (!path || !out_count)
            return -1;
        if (!entries || max_entries <= 0)
            return 0;

        DIR *dir = opendir(path);
        if (!dir)
            return -errno;

        struct dirent *ent = nullptr;
        while ((ent = readdir(dir)) != nullptr) {
            if (is_dot_entry(ent->d_name))
                continue;
            if (*out_count >= max_entries)
                break;
            cp0_dirent_t &entry = entries[*out_count];
            copy_cstr(entry.name, sizeof(entry.name), ent->d_name);
            entry.is_dir = ent->d_type == DT_DIR ? 1 : 0;
            if (ent->d_type == DT_UNKNOWN) {
                std::string child = join_path(path, ent->d_name);
                struct stat st;
                entry.is_dir = stat(child.c_str(), &st) == 0 && S_ISDIR(st.st_mode) ? 1 : 0;
            }
            ++(*out_count);
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
            struct dirent *ent = nullptr;
            while ((ent = readdir(dir)) != nullptr) {
                if (is_dot_entry(ent->d_name)) continue;
                bool is_dir = ent->d_type == DT_DIR;
                if (ent->d_type == DT_UNKNOWN) {
                    struct stat st{};
                    is_dir = stat(join_path(path, ent->d_name).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
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
        if (!dir) return -errno;
        try {
            std::ostringstream out;
            struct dirent *ent = nullptr;
            while ((ent = readdir(dir)) != nullptr) {
                if (is_dot_entry(ent->d_name)) continue;
                const std::string full = join_path(path, ent->d_name);
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

    static void snapshot(SdlWatcher *watcher)
    {
        if (!watcher)
            return;
        struct stat st;
        if (stat(watcher->path.c_str(), &st) == 0) {
            watcher->mtime = st.st_mtime;
            watcher->size = st.st_size;
            watcher->nlink = st.st_nlink;
            watcher->valid = true;
        } else {
            watcher->valid = false;
        }
    }

    static cp0_watcher_t watch_start(const char *path)
    {
        if (!path || !path[0])
            return nullptr;
        SdlWatcher *watcher = new SdlWatcher();
        watcher->path = path;
        snapshot(watcher);
        return watcher;
    }

    static int watch_poll(cp0_watcher_t handle)
    {
        if (!handle)
            return -1;
        SdlWatcher *watcher = static_cast<SdlWatcher *>(handle);
        SdlWatcher now;
        now.path = watcher->path;
        snapshot(&now);
        const bool changed = now.valid != watcher->valid || now.mtime != watcher->mtime ||
                             now.size != watcher->size || now.nlink != watcher->nlink;
        *watcher = now;
        return changed ? 1 : 0;
    }

    static void watch_stop(cp0_watcher_t watcher)
    {
        delete static_cast<SdlWatcher *>(watcher);
    }
};

SdlFilesystem &filesystem()
{
    static SdlFilesystem instance;
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
    std::string key = file ? file : "";
    auto it = paths.find(key);
    if (it == paths.end())
        it = paths.emplace(key, cp0_file_path(key)).first;
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
        filesystem().api_call(std::move(arg), std::move(callback));
    });
}
