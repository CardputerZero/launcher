#include "cp0_lvgl_app.h"
#include "hal/hal_paths.h"
#include "hal_lvgl_bsp.h"

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace {

constexpr int kMaxEntries = 64;
constexpr int kKeyMax = 64;
constexpr int kValMax = 256;

static void copy_cstr(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0)
        return;
    if (!src)
        src = "";
    std::strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static std::string join_path(const std::string &base, const char *leaf)
{
    std::string out = base.empty() ? std::string(".") : base;
    if (!out.empty() && out.back() != '/')
        out.push_back('/');
    out += leaf;
    return out;
}

static int mkdir_p(const std::string &path)
{
    if (path.empty())
        return -1;
    char tmp[512];
    copy_cstr(tmp, sizeof(tmp), path.c_str());
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    return (mkdir(tmp, 0755) == 0 || errno == EEXIST) ? 0 : -1;
}

static std::string config_dir()
{
    const char *env = std::getenv("APPLAUNCH_SDL_CONFIG_DIR");
    if (env && env[0])
        return env;
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0])
        return join_path(xdg, "applaunch-sdl");
    const char *home = std::getenv("HOME");
    if (home && home[0])
        return join_path(join_path(home, ".config"), "applaunch-sdl");
    return join_path(hal_path_data_dir() ? hal_path_data_dir() : ".", "sdl_config");
}

class ConfigSystem {
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    ConfigSystem() { init(); }

    void init()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        load_locked();
    }

    int get_int(const char *key, int default_val)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_loaded_locked();
        int idx = find_entry_locked(key);
        return idx < 0 ? default_val : std::atoi(entries_[idx].val);
    }

    void set_int(const char *key, int val)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", val);
        set_str(key, buf);
    }

    std::string get_str(const char *key, const char *default_val)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_loaded_locked();
        int idx = find_entry_locked(key);
        return idx < 0 ? std::string(default_val ? default_val : "") : std::string(entries_[idx].val);
    }

    void set_str(const char *key, const char *val)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_loaded_locked();
        int idx = get_or_create_entry_locked(key);
        if (idx >= 0)
            copy_cstr(entries_[idx].val, sizeof(entries_[idx].val), val);
    }

    int save()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_loaded_locked();
        const std::string dir = config_dir();
        if (mkdir_p(dir) != 0)
            return -1;
        FILE *fp = std::fopen(join_path(dir, "settings").c_str(), "w");
        if (!fp)
            return -1;
        for (int i = 0; i < count_; ++i)
            std::fprintf(fp, "%s=%s\n", entries_[i].key, entries_[i].val);
        std::fclose(fp);
        return 0;
    }

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty()) {
            report(callback, -1, "empty config api");
            return;
        }
        const std::string cmd = arg.front();
        if (cmd == "Init") {
            init();
            report(callback, 0, "ok");
        } else if (cmd == "GetInt") {
            report(callback, 0, std::to_string(get_int(nth_arg(arg, 1).c_str(), parse_int(nth_arg(arg, 2), 0))));
        } else if (cmd == "SetInt") {
            set_int(nth_arg(arg, 1).c_str(), parse_int(nth_arg(arg, 2), 0));
            report(callback, 0, "ok");
        } else if (cmd == "GetStr") {
            report(callback, 0, get_str(nth_arg(arg, 1).c_str(), nth_arg(arg, 2).c_str()));
        } else if (cmd == "SetStr") {
            set_str(nth_arg(arg, 1).c_str(), nth_arg(arg, 2).c_str());
            report(callback, 0, "ok");
        } else if (cmd == "Save") {
            int ret = save();
            report(callback, ret, ret == 0 ? "ok" : "save failed");
        } else {
            report(callback, -1, "unknown config api");
        }
    }

private:
    struct Entry {
        char key[kKeyMax];
        char val[kValMax];
    };

    Entry entries_[kMaxEntries] = {};
    int count_ = 0;
    bool loaded_ = false;
    std::mutex mutex_;

    void ensure_loaded_locked()
    {
        if (!loaded_)
            load_locked();
    }

    void load_locked()
    {
        count_ = 0;
        loaded_ = true;
        FILE *fp = std::fopen(join_path(config_dir(), "settings").c_str(), "r");
        if (!fp)
            return;
        char line[kKeyMax + kValMax + 4];
        while (std::fgets(line, sizeof(line), fp) && count_ < kMaxEntries) {
            line[std::strcspn(line, "\r\n")] = '\0';
            char *eq = std::strchr(line, '=');
            if (!eq || line[0] == '\0')
                continue;
            *eq = '\0';
            copy_cstr(entries_[count_].key, sizeof(entries_[count_].key), line);
            copy_cstr(entries_[count_].val, sizeof(entries_[count_].val), eq + 1);
            ++count_;
        }
        std::fclose(fp);
    }

    int find_entry_locked(const char *key) const
    {
        if (!key || !key[0])
            return -1;
        for (int i = 0; i < count_; ++i) {
            if (std::strcmp(entries_[i].key, key) == 0)
                return i;
        }
        return -1;
    }

    int get_or_create_entry_locked(const char *key)
    {
        int idx = find_entry_locked(key);
        if (idx >= 0)
            return idx;
        if (!key || !key[0] || count_ >= kMaxEntries)
            return -1;
        idx = count_++;
        copy_cstr(entries_[idx].key, sizeof(entries_[idx].key), key);
        entries_[idx].val[0] = '\0';
        return idx;
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        for (size_t i = 0; i < index && it != arg.end(); ++i)
            ++it;
        return it == arg.end() ? std::string() : *it;
    }

    static int parse_int(const std::string &value, int fallback)
    {
        return value.empty() ? fallback : std::atoi(value.c_str());
    }

    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }
};

} // namespace

extern "C" void init_config(void)
{
    auto config = std::make_shared<ConfigSystem>();
    cp0_signal_config_api.append([config](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        config->api_call(std::move(arg), std::move(callback));
    });
}
