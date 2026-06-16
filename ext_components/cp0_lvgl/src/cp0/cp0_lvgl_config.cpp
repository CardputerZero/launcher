#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

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

#define CONFIG_DIR  "/var/lib/applaunch"
#define CONFIG_FILE CONFIG_DIR "/settings"
#define MAX_ENTRIES 32
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
        mkdir(CONFIG_DIR, 0755);
        FILE *fp = std::fopen(CONFIG_FILE, "w");
        if (!fp) return;
        for (int i = 0; i < count_; i++) {
            std::fprintf(fp, "%s=%s\n", entries_[i].key, entries_[i].val);
        }
        std::fclose(fp);
        sync();
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

    void ensure_loaded_locked()
    {
        if (!loaded_) load_locked();
    }

    void load_locked()
    {
        count_ = 0;
        loaded_ = true;

        FILE *fp = std::fopen(CONFIG_FILE, "r");
        if (!fp) return;

        char line[KEY_MAX + VAL_MAX + 4];
        while (std::fgets(line, sizeof(line), fp) && count_ < MAX_ENTRIES) {
            line[std::strcspn(line, "\r\n")] = 0;
            char *eq = std::strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            if (line[0] == '\0') continue;
            copy_cstr(entries_[count_].key, line, KEY_MAX);
            copy_cstr(entries_[count_].val, eq + 1, VAL_MAX);
            count_++;
        }
        std::fclose(fp);
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
