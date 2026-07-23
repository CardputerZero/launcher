#include "cp0_lvgl_app.h"
#include "hal/hal_paths.h"
#include "hal_lvgl_bsp.h"

#include <cassert>
#include <list>
#include <string>

eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_filesystem_api;

extern "C" void init_filesystem(void);

namespace {

struct Reply {
    int code = -999;
    std::string data;
    int callbacks = 0;
};

Reply call(std::list<std::string> arguments)
{
    Reply reply;
    cp0_signal_filesystem_api(std::move(arguments), [&](int code, std::string data) {
        reply.code = code;
        reply.data = std::move(data);
        ++reply.callbacks;
    });
    return reply;
}

} // namespace

int main()
{
    hal_paths_init("/opt/test-data");
    init_filesystem();
    init_filesystem();

    Reply missing = call({});
    assert(missing.callbacks == 1 && missing.code == -1);
    assert(cp0_file_path("icon.PNG") == "/opt/test-data/APPLaunch/share/images/icon.PNG");
    assert(cp0_file_path("APPLaunch/share/font/a.ttf") ==
           "/opt/test-data/APPLaunch/share/font/a.ttf");
    assert(cp0_file_path("share/audio/tone.ogg") ==
           "/opt/test-data/APPLaunch/share/audio/tone.ogg");
    assert(cp0_file_path("share/images/../secret.png").empty());
    assert(cp0_file_path("plain.txt") == "plain.txt");

    Reply started = call({"WatchStart", "/tmp"});
    assert(started.code == 0 && !started.data.empty());
    Reply polled = call({"WatchPoll", started.data});
    assert(polled.code == 0 && (polled.data == "0" || polled.data == "1"));
    assert(call({"WatchPoll", "123456789"}).code == -1);
    assert(call({"WatchStop", started.data}).code == 0);
    assert(call({"WatchStop", started.data}).code == -1);

    cp0_watcher_t watcher = cp0_dir_watch_start("/tmp");
    assert(watcher != nullptr && cp0_dir_watch_poll(watcher) >= 0);
    cp0_dir_watch_stop(watcher);
    cp0_dir_watch_stop(watcher);
    assert(cp0_dir_watch_poll(watcher) == -1);
    return 0;
}
