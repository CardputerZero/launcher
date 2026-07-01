#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include "../cp0_config_json.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

// Unified per-user device config. Everything the launcher persists (app
// visibility, brightness, camera resolution, ...) lives in a single JSON file
// under the user's home, shared with other apps (e.g. the camera app reads
// camera.resolution.{width,height} from here).
#define MAX_ENTRIES 64
#define KEY_MAX     64
#define VAL_MAX     256

class ConfigSystem
{
public:
    typedef std::function<void(int, std::string)> callback_t;
    typedef std::list<std::string> arg_t;

    ConfigSystem()
    {
        init();
    }
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
        if (idx < 0) return default_val;
        return std::atoi(entries_[idx].val);
    }

    void set_int(const char *key, int val)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_loaded_locked();
        int idx = get_or_create_entry_locked(key);
        if (idx < 0) return;
        std::snprintf(entries_[idx].val, VAL_MAX, "%d", val);
    }

    const char *get_str(const char *key, const char *default_val)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_loaded_locked();
        int idx = find_entry_locked(key);
        if (idx < 0) return default_val;
        return entries_[idx].val;
    }

    void set_str(const char *key, const char *val)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_loaded_locked();
        int idx = get_or_create_entry_locked(key);
        if (idx < 0) return;
        copy_cstr(entries_[idx].val, val ? val : "", VAL_MAX);
    }

    void save()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_loaded_locked();
        save_locked();
    }

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty()) {
            report(callback, -1, "empty config api\n");
            return;
        }

        const std::string cmd = arg.front();
        if (cmd == "Init") {
            init();
            report(callback, 0, "ok");
        } else if (cmd == "GetInt") {
            const std::string key = nth_arg(arg, 1);
            int default_val = parse_int(nth_arg(arg, 2), 0);
            int val = get_int(key.c_str(), default_val);
            report(callback, 0, std::to_string(val));
        } else if (cmd == "SetInt") {
            const std::string key = nth_arg(arg, 1);
            int val = parse_int(nth_arg(arg, 2), 0);
            set_int(key.c_str(), val);
            report(callback, 0, "ok");
        } else if (cmd == "GetStr") {
            const std::string key = nth_arg(arg, 1);
            const std::string default_val = nth_arg(arg, 2);
            report(callback, 0, get_str(key.c_str(), default_val.c_str()));
        } else if (cmd == "SetStr") {
            const std::string key = nth_arg(arg, 1);
            const std::string val = nth_arg(arg, 2);
            set_str(key.c_str(), val.c_str());
            report(callback, 0, "ok");
        } else if (cmd == "Save") {
            save();
            report(callback, 0, "ok");
        } else {
            report(callback, -1, "unknown config api\n");
        }
    }

private:
    struct Entry {
        char key[KEY_MAX];
        char val[VAL_MAX];
    };

    Entry entries_[MAX_ENTRIES] = {};
    int count_ = 0;
    bool loaded_ = false;
    std::mutex mutex_;

    static std::string config_dir()
    {
        const char *home = std::getenv("HOME");
        std::string base = (home && home[0]) ? std::string(home) : std::string("/root");
        return base + "/.config/cardputerzero";
    }

    static std::string config_file()
    {
        return config_dir() + "/config.json";
    }

    void ensure_loaded_locked()
    {
        if (!loaded_) load_locked();
    }

    void add_entry_locked(const std::string &key, const std::string &val)
    {
        if (key.empty() || count_ >= MAX_ENTRIES) return;
        copy_cstr(entries_[count_].key, key.c_str(), KEY_MAX);
        copy_cstr(entries_[count_].val, val.c_str(), VAL_MAX);
        count_++;
    }

    void load_locked()
    {
        count_ = 0;
        loaded_ = true;

        std::string text;
        if (!read_file(config_file().c_str(), text))
            return;
        std::vector<std::pair<std::string, std::string>> kv;
        if (!cp0cfg::from_json(text, kv))
            return;
        for (const auto &e : kv)
            add_entry_locked(e.first, e.second);
    }

    void save_locked()
    {
        std::vector<std::pair<std::string, std::string>> kv;
        kv.reserve(count_);
        for (int i = 0; i < count_; i++)
            kv.emplace_back(entries_[i].key, entries_[i].val);
        const std::string json = cp0cfg::to_json(kv);

        const std::string dir = config_dir();
        const char *home = std::getenv("HOME");
        std::string base = (home && home[0]) ? std::string(home) : std::string("/root");
        mkdir((base + "/.config").c_str(), 0755);
        mkdir(dir.c_str(), 0755);

        FILE *fp = std::fopen(config_file().c_str(), "w");
        if (!fp) return;
        std::fwrite(json.data(), 1, json.size(), fp);
        std::fclose(fp);
        sync();
    }

    static bool read_file(const char *path, std::string &out)
    {
        FILE *fp = std::fopen(path, "r");
        if (!fp) return false;
        char buf[512];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0)
            out.append(buf, n);
        std::fclose(fp);
        return true;
    }

    int find_entry_locked(const char *key) const
    {
        if (!key || key[0] == '\0') return -1;
        for (int i = 0; i < count_; i++) {
            if (std::strcmp(entries_[i].key, key) == 0) return i;
        }
        return -1;
    }

    int get_or_create_entry_locked(const char *key)
    {
        int idx = find_entry_locked(key);
        if (idx >= 0) return idx;
        if (!key || key[0] == '\0' || count_ >= MAX_ENTRIES) return -1;
        idx = count_++;
        copy_cstr(entries_[idx].key, key, KEY_MAX);
        entries_[idx].val[0] = '\0';
        return idx;
    }

    static void copy_cstr(char *dst, const char *src, size_t dst_size)
    {
        if (!dst || dst_size == 0) return;
        if (!src) src = "";
        std::strncpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
    }

    static std::string nth_arg(const arg_t& arg, size_t index)
    {
        auto it = arg.begin();
        for (size_t i = 0; i < index && it != arg.end(); i++) ++it;
        return it == arg.end() ? std::string() : *it;
    }

    static int parse_int(const std::string& value, int default_val)
    {
        if (value.empty()) return default_val;
        return std::atoi(value.c_str());
    }

    static void report(callback_t callback, int code, const std::string& data)
    {
        if (callback) callback(code, data);
    }
};

extern "C" {

void init_config(void)
{
    std::shared_ptr<ConfigSystem> config = std::make_shared<ConfigSystem>();
    cp0_signal_config_api.append([config](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        config->api_call(std::move(arg), std::move(callback));
    });
}

}
