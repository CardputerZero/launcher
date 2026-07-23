#include "../src/cp0_filesystem_api.hpp"
#include "../src/cp0_posix_filesystem.hpp"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <list>
#include <stdexcept>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

int main()
{
    char directory_template[] = "/tmp/cp0-posix-filesystem-XXXXXX";
    const char *directory = mkdtemp(directory_template);
    assert(directory);

    const std::string file_path = std::string(directory) + "/a%\tb";
    FILE *file = std::fopen(file_path.c_str(), "wb");
    assert(file);
    assert(std::fwrite("hello", 1, 5, file) == 5);
    assert(std::fclose(file) == 0);
    const std::string child_directory = std::string(directory) + "/child";
    assert(mkdir(child_directory.c_str(), 0700) == 0);

    cp0_dirent_t entries[4]{};
    int count = 0;
    assert(cp0_posix_filesystem::list_directory(directory, entries, 4, &count, true) == 0);
    assert(count == 2);
    bool found_file = false;
    bool found_directory = false;
    for (int index = 0; index < count; ++index) {
        if (std::strcmp(entries[index].name, "a%\tb") == 0) found_file = !entries[index].is_dir;
        if (std::strcmp(entries[index].name, "child") == 0)
            found_directory = entries[index].is_dir;
    }
    assert(found_file && found_directory);

    std::string encoded;
    assert(cp0_posix_filesystem::encode_directory(directory, false, true, encoded) == 0);
    assert(encoded.find("F\ta%25%09b\n") != std::string::npos);
    assert(encoded.find("D\tchild\n") != std::string::npos);
    assert(cp0_posix_filesystem::encode_directory(directory, true, true, encoded) == 0);
    assert(encoded.find("F\t5\ta%25%09b\n") != std::string::npos);

    std::string contents;
    assert(cp0_posix_filesystem::read_file_limited(file_path, 5, contents) == 0);
    assert(contents == "hello");
    assert(cp0_posix_filesystem::read_file_limited(file_path, 4, contents) == -EFBIG);

    const std::string missing = std::string(directory) + "/missing";
    assert(cp0_posix_filesystem::list_directory(missing.c_str(), entries, 4, &count, false) == -1);
    assert(cp0_posix_filesystem::list_directory(missing.c_str(), entries, 4, &count, true) ==
           -ENOENT);

    cp0_watcher_t watched = reinterpret_cast<cp0_watcher_t>(static_cast<uintptr_t>(0x1234));
    bool watch_stopped = false;
    cp0_filesystem_api::WatcherRegistry watchers;
    cp0_filesystem_api::Operations operations;
    operations.resolve_path = [](const std::string &value) { return "resolved/" + value; };
    operations.ensure_directory = [](const std::string &path) {
        return mkdir(path.c_str(), 0700) == 0 ? 0 : -1;
    };
    operations.watch_start = [&](const char *) { return watchers.add(watched); };
    operations.watch_poll = [&](cp0_watcher_t watcher) {
        return watchers.poll(watcher, [watched](cp0_watcher_t active) {
            return active == watched ? 1 : -1;
        });
    };
    operations.watch_stop = [&](cp0_watcher_t watcher) {
        return watchers.stop(watcher, [&](cp0_watcher_t active) {
            watch_stopped = active == watched;
        }) ? 0 : -1;
    };

    auto invoke = [&](std::list<std::string> arguments, int &code, std::string &data) {
        cp0_filesystem_api::dispatch(arguments, operations, [&](int result, std::string output) {
            code = result;
            data = std::move(output);
        });
    };
    int code = -99;
    std::string data;
    invoke({"Path", "asset.png"}, code, data);
    assert(code == 0 && data == "resolved/asset.png");
    invoke({"DirList", directory}, code, data);
    assert(code == 0 && data.find("F\ta%25%09b\n") != std::string::npos);
    invoke({"DirListDetail", directory}, code, data);
    assert(code == 0 && data.find("F\t5\ta%25%09b\n") != std::string::npos);
    invoke({"Exists", file_path}, code, data);
    assert(code == 0 && data == "1");
    invoke({"ReadFile", file_path, "5"}, code, data);
    assert(code == 0 && data == "hello");
    invoke({"ReadFile", file_path, "4"}, code, data);
    assert(code == -EFBIG);
    invoke({"ReadFile", file_path, "0"}, code, data);
    assert(code == -EFBIG);
    invoke({"ReadFile", file_path, "5junk"}, code, data);
    assert(code == -1);
    invoke({"ReadFile", file_path, "-1"}, code, data);
    assert(code == -1);
    invoke({"ReadFile", file_path, "184467440737095516160"}, code, data);
    assert(code == -1);

    const std::string ensured = std::string(directory) + "/ensured";
    invoke({"EnsureDirForUser", ensured}, code, data);
    assert(code == 0);
    const std::string touched = std::string(directory) + "/touched";
    invoke({"Touch", touched}, code, data);
    assert(code == 0 && access(touched.c_str(), F_OK) == 0);
    invoke({"Remove", touched}, code, data);
    assert(code == 0 && access(touched.c_str(), F_OK) != 0);
    invoke({"WatchStart", directory}, code, data);
    assert(code == 0 && !data.empty());
    const std::string watcher_handle = data;
    invoke({"WatchPoll", watcher_handle}, code, data);
    assert(code == 0 && data == "1");
    invoke({"WatchStop", watcher_handle}, code, data);
    assert(code == 0 && watch_stopped);
    invoke({"WatchPoll", watcher_handle}, code, data);
    assert(code == -1 && data == "-1");
    invoke({"WatchStop", watcher_handle}, code, data);
    assert(code == -1);
    invoke({"WatchPoll", "0x1234"}, code, data);
    assert(code == -1);
    invoke({"WatchPoll", "4660junk"}, code, data);
    assert(code == -1);
    invoke({"WatchPoll", "184467440737095516160"}, code, data);
    assert(code == -1);

    cp0_watcher_t decoded_watcher = nullptr;
    assert(cp0_filesystem_api::decode_watcher_handle("4660", decoded_watcher));
    assert(decoded_watcher == watched);
    assert(cp0_filesystem_api::encode_watcher_handle(watched) == "4660");
    assert(!cp0_filesystem_api::decode_watcher_handle("+4660", decoded_watcher));
    assert(!cp0_filesystem_api::decode_watcher_handle("4660 ", decoded_watcher));

    invoke({"Exists"}, code, data);
    assert(code == -1);
    invoke({"Touch", ""}, code, data);
    assert(code == -1);
    invoke({"DirList", directory, "extra"}, code, data);
    assert(code == -1);
    invoke({"Path", "asset.png", "extra"}, code, data);
    assert(code == -1);
    invoke({"Exists", std::string("bad\0path", 8)}, code, data);
    assert(code == -1);
    invoke({"Unknown"}, code, data);
    assert(code == -2 && data == "unknown command: Unknown");

    int callback_calls = 0;
    cp0_filesystem_api::Operations throwing_operations = operations;
    throwing_operations.resolve_path = [](const std::string &) -> std::string {
        throw std::runtime_error("resolve failed");
    };
    cp0_filesystem_api::dispatch({"Path", "asset.png"}, throwing_operations,
        [&](int result, std::string output) {
            ++callback_calls;
            code = result;
            data = std::move(output);
        });
    assert(callback_calls == 1 && code == -1 && data == "filesystem operation failed");

    throwing_operations = operations;
    throwing_operations.watch_poll = [](cp0_watcher_t) -> int {
        throw std::runtime_error("backend failed");
    };
    callback_calls = 0;
    cp0_filesystem_api::dispatch({"WatchPoll", "4660"}, throwing_operations,
        [&](int result, std::string output) {
            ++callback_calls;
            code = result;
            data = std::move(output);
        });
    assert(callback_calls == 1 && code == -1 && data == "filesystem operation failed");

    callback_calls = 0;
    cp0_filesystem_api::dispatch({"Exists", file_path}, operations,
        [&](int, std::string) {
            ++callback_calls;
            throw std::runtime_error("user callback failed");
        });
    assert(callback_calls == 1);

    assert(unlink(file_path.c_str()) == 0);
    assert(rmdir(ensured.c_str()) == 0);
    assert(rmdir(child_directory.c_str()) == 0);
    assert(rmdir(directory) == 0);
}
