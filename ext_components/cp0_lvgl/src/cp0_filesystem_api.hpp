#pragma once

#include "cp0_lvgl_app.h"

#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace cp0_filesystem_api {

using Callback = std::function<void(int, std::string)>;

struct Operations {
    std::function<std::string(const std::string &)> resolve_path;
    std::function<int(const std::string &)> ensure_directory;
    std::function<cp0_watcher_t(const char *)> watch_start;
    std::function<int(cp0_watcher_t)> watch_poll;
    std::function<int(cp0_watcher_t)> watch_stop;
    bool detail_errno_on_open_failure = true;
};

class WatcherRegistry {
public:
    cp0_watcher_t add(cp0_watcher_t watcher);

    template <typename Poll>
    int poll(cp0_watcher_t watcher, Poll operation)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const uintptr_t key = reinterpret_cast<uintptr_t>(watcher);
        const auto it = watchers_.find(key);
        if (it == watchers_.end()) return -1;
        return operation(it->second);
    }

    template <typename Stop>
    bool stop(cp0_watcher_t watcher, Stop operation)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const uintptr_t key = reinterpret_cast<uintptr_t>(watcher);
        const auto it = watchers_.find(key);
        if (it == watchers_.end()) return false;
        cp0_watcher_t native = it->second;
        watchers_.erase(it);
        operation(native);
        return true;
    }

private:
    std::mutex mutex_;
    std::unordered_map<uintptr_t, cp0_watcher_t> watchers_;
    uintptr_t next_handle_ = 1;
};

std::string encode_watcher_handle(cp0_watcher_t watcher);
bool decode_watcher_handle(const std::string &text, cp0_watcher_t &watcher);

void dispatch(const std::list<std::string> &arguments, const Operations &operations,
              Callback callback);

} // namespace cp0_filesystem_api
