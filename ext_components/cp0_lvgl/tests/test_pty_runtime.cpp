#include "hal_lvgl_bsp.h"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>

eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_pty_api;
eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_config_api;

extern "C" void init_pty(void);
extern "C" void deinit_pty(void) noexcept;

namespace {

struct Reply {
    int code = -999;
    std::string data;
    int callbacks = 0;
};

Reply call(std::list<std::string> arguments)
{
    Reply reply;
    cp0_signal_pty_api(std::move(arguments), [&](int code, std::string data) {
        reply.code = code;
        reply.data = std::move(data);
        ++reply.callbacks;
    });
    return reply;
}

} // namespace

int main()
{
    init_pty();
    init_pty();

    Reply invalid = call({});
    assert(invalid.callbacks == 1 && invalid.code == -1);

    Reply opened = call({"Open", "/bin/sh", "80", "24", "/bin/sh", "-c",
                         "printf ready; sleep 0.05; exit 7"});
    assert(opened.callbacks == 1 && opened.code == 0 && !opened.data.empty());
    const std::string handle = opened.data;

    Reply resized = call({"Resize", handle, "100", "30"});
    assert(resized.code == 0);

    std::string output;
    for (int attempt = 0; attempt < 200 && output.find("ready") == std::string::npos; ++attempt) {
        Reply read = call({"Read", handle, "128"});
        assert(read.code >= 0);
        output += read.data;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(output.find("ready") != std::string::npos);

    Reply child;
    for (int attempt = 0; attempt < 200; ++attempt) {
        child = call({"CheckChild", handle});
        if (child.code == 1) break;
        assert(child.code == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(child.code == 1 && child.data == "7");
    Reply repeated_check = call({"CheckChild", handle});
    assert(repeated_check.code == 1 && repeated_check.data == "7");

    assert(call({"Close", handle}).code == 0);
    assert(call({"Close", handle}).code == -1);
    assert(call({"Read", "999999999", "16"}).code == -1);

    Reply active = call({"Open", "/bin/sh", "80", "24", "/bin/sh", "-c", "sleep 30"});
    assert(active.code == 0 && !active.data.empty());
    const std::string stale_handle = active.data;
    deinit_pty();
    deinit_pty();
    assert(call({"Read", stale_handle, "16"}).callbacks == 0);

    init_pty();
    Reply stale = call({"Read", stale_handle, "16"});
    assert(stale.callbacks == 1 && stale.code == -1);
    Reply reopened = call({"Open", "/bin/sh", "80", "24", "/bin/sh", "-c", "exit 0"});
    assert(reopened.callbacks == 1 && reopened.code == 0);
    assert(call({"Close", reopened.data}).code == 0);
    deinit_pty();
    assert(call({}).callbacks == 0);
    return 0;
}
