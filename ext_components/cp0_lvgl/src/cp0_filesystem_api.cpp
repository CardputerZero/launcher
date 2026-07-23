#include "cp0_filesystem_api.hpp"

#include "cp0_callback_result.hpp"
#include "cp0_integer_codec.hpp"
#include "cp0_posix_filesystem.hpp"

#include <cerrno>
#include <cstdio>
#include <iterator>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

namespace cp0_filesystem_api {
namespace {

bool parse_size(const std::string &value, size_t &size)
{
    return cp0_integer_codec::parse_decimal(
        value, size_t{0}, std::numeric_limits<size_t>::max(), size);
}

bool valid_path(const std::string &path)
{
    return !path.empty() && path.find('\0') == std::string::npos;
}

int touch_file(const std::string &path)
{
    const auto slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
        const std::string parent = path.substr(0, slash);
        struct stat status{};
        if ((mkdir(parent.c_str(), 0755) != 0 && errno != EEXIST) ||
            stat(parent.c_str(), &status) != 0 || !S_ISDIR(status.st_mode)) {
            return -1;
        }
    }

    FILE *file = fopen(path.c_str(), "a");
    if (!file) return -1;
    fclose(file);
    return 0;
}

} // namespace

cp0_watcher_t WatcherRegistry::add(cp0_watcher_t watcher)
{
    if (!watcher) return nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    while (next_handle_ == 0 || watchers_.count(next_handle_) != 0) ++next_handle_;
    const uintptr_t handle = next_handle_++;
    watchers_.emplace(handle, watcher);
    return reinterpret_cast<cp0_watcher_t>(handle);
}

std::string encode_watcher_handle(cp0_watcher_t watcher)
{
    const uintptr_t value = reinterpret_cast<uintptr_t>(watcher);
    return value == 0 ? std::string() : std::to_string(value);
}

bool decode_watcher_handle(const std::string &text, cp0_watcher_t &watcher)
{
    watcher = nullptr;
    uintptr_t parsed = 0;
    if (!cp0_integer_codec::parse_decimal(
            text, uintptr_t{1}, std::numeric_limits<uintptr_t>::max(), parsed))
        return false;
    watcher = reinterpret_cast<cp0_watcher_t>(parsed);
    return true;
}

void dispatch(const std::list<std::string> &arguments, const Operations &operations,
              Callback callback)
{
    cp0::CallbackResult result(std::move(callback));
    auto report = [&](int code, std::string data) {
        result.complete(code, data);
    };
    try {
        if (arguments.empty()) {
            report(-1, "missing command");
            return;
        }

        const std::string &command = arguments.front();
        const std::string value =
            arguments.size() >= 2 ? *std::next(arguments.begin()) : std::string();
        if (command == "Path") {
            if (arguments.size() != 2 || value.find('\0') != std::string::npos ||
                !operations.resolve_path) {
                report(-1, "invalid Path arguments");
                return;
            }
            report(0, operations.resolve_path(value));
        } else if (command == "DirList" || command == "DirListDetail") {
            if (arguments.size() != 2 || !valid_path(value)) {
                report(-1, "invalid directory path");
                return;
            }
            const bool detail = command == "DirListDetail";
            std::string data;
            const int code = cp0_posix_filesystem::encode_directory(
                value.c_str(), detail,
                detail ? operations.detail_errno_on_open_failure : true, data);
            report(code, std::move(data));
        } else if (command == "Exists") {
            if (arguments.size() != 2 || !valid_path(value)) {
                report(-1, "invalid filesystem path");
                return;
            }
            report(0, access(value.c_str(), R_OK) == 0 ? "1" : "0");
        } else if (command == "ReadFile") {
            if ((arguments.size() != 2 && arguments.size() != 3) || !valid_path(value)) {
                report(-1, "invalid ReadFile arguments");
                return;
            }
            const auto max_it =
                arguments.size() >= 3 ? std::next(arguments.begin(), 2) : arguments.end();
            size_t max_bytes = std::numeric_limits<size_t>::max();
            if (max_it != arguments.end() && !parse_size(*max_it, max_bytes)) {
                report(-1, "invalid ReadFile size");
                return;
            }
            std::string data;
            const int code = cp0_posix_filesystem::read_file_limited(value, max_bytes, data);
            report(code, std::move(data));
        } else if (command == "EnsureDirForUser") {
            if (arguments.size() != 2 || !valid_path(value) || !operations.ensure_directory) {
                report(-1, "invalid directory path");
                return;
            }
            report(operations.ensure_directory(value), "");
        } else if (command == "Touch") {
            if (arguments.size() != 2 || !valid_path(value)) {
                report(-1, "invalid filesystem path");
                return;
            }
            report(touch_file(value), "");
        } else if (command == "Remove") {
            if (arguments.size() != 2 || !valid_path(value)) {
                report(-1, "invalid filesystem path");
                return;
            }
            report(std::remove(value.c_str()) == 0 ? 0 : -1, "");
        } else if (command == "WatchStart") {
            if (arguments.size() != 2 || !valid_path(value) || !operations.watch_start) {
                report(-1, "invalid watch path");
                return;
            }
            const cp0_watcher_t watcher = operations.watch_start(value.c_str());
            report(watcher ? 0 : -1, encode_watcher_handle(watcher));
        } else if (command == "WatchPoll") {
            cp0_watcher_t watcher = nullptr;
            if (arguments.size() != 2 || !decode_watcher_handle(value, watcher) ||
                !operations.watch_poll) {
                report(-1, "invalid watcher handle");
                return;
            }
            const int poll_result = operations.watch_poll(watcher);
            report(poll_result < 0 ? poll_result : 0, std::to_string(poll_result));
        } else if (command == "WatchStop") {
            cp0_watcher_t watcher = nullptr;
            if (arguments.size() != 2 || !decode_watcher_handle(value, watcher) ||
                !operations.watch_stop) {
                report(-1, "invalid watcher handle");
                return;
            }
            report(operations.watch_stop(watcher), "");
        } else {
            report(-2, "unknown command: " + command);
        }
    } catch (...) {
        report(-1, "filesystem operation failed");
    }
}

} // namespace cp0_filesystem_api
