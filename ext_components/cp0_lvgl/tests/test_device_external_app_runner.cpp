// SPDX-License-Identifier: MIT
#include "../src/cp0/cp0_external_app_runner.hpp"
#include "../src/cp0/cp0_process_commands.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <string>
#include <thread>
#include <unistd.h>

namespace cp0_process_commands {
void exec_shell_as_configured_user(const char *) { std::abort(); }
}

int main()
{
    // No keyboard events: both a direct exit and a later orphan exit must wake the runner.
    alarm(15);
    int keys[2];
    assert(pipe2(keys, O_CLOEXEC) == 0);
    const std::string keyboard = "/proc/self/fd/" + std::to_string(keys[0]);
    assert(setenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE", keyboard.c_str(), 1) == 0);
    assert(cp0_external_app_runner::run("exit 17", true) == 17);

    int marker[2];
    assert(pipe(marker) == 0);
    const std::string command = "(sleep 0.2; printf x > /proc/self/fd/" + std::to_string(marker[1]) + ") & exit 7";
    assert(cp0_external_app_runner::run(command.c_str(), true) == 7);
    pollfd ready{marker[0], POLLIN, 0};
    assert(poll(&ready, 1, 0) == 1 && (ready.revents & POLLIN));
    char byte;
    assert(read(marker[0], &byte, 1) == 1);
    std::thread press([&] {
        // The new app signals after setting its TERM disposition.
        assert(read(marker[0], &byte, 1) == 1);
        input_event esc{};
        esc.type = EV_KEY;
        esc.code = KEY_ESC;
        esc.value = 1;
        assert(write(keys[1], &esc, sizeof(esc)) == sizeof(esc));
    });
    const auto before = std::chrono::steady_clock::now();
    const std::string ignore_term = "trap '' TERM; printf R > /proc/self/fd/" +
        std::to_string(marker[1]) + "; exec sleep 60";
    assert(cp0_external_app_runner::run(ignore_term.c_str(), true) == -1);
    press.join();
    const auto held = std::chrono::steady_clock::now() - before;
    assert(held >= std::chrono::seconds(6) && held < std::chrono::seconds(10));
    close(marker[0]);
    close(marker[1]);
    close(keys[0]);
    close(keys[1]);
}
