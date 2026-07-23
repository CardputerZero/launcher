#include "cp0_external_app_runner.hpp"

#include "../cp0_external_process_group.hpp"
#include "cp0_process_commands.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#if !defined(_WIN32)
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern "C" void __attribute__((weak)) keyboard_pause(void) {}
extern "C" void __attribute__((weak)) keyboard_resume(void) {}

namespace cp0_external_app_runner {
namespace {

const char *keyboard_device()
{
    const char *configured = std::getenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE");
    return configured ? configured : "/dev/input/by-path/platform-3f804000.i2c-event";
}

} // namespace

int run(const char *command, volatile int *home_key_flag, bool keep_root)
{
#if defined(_WIN32)
    (void)command;
    (void)home_key_flag;
    (void)keep_root;
    return -1;
#else
    keyboard_pause();
    const bool subreaper = cp0_process_group::enable_subreaper();

    const int keyboard_fd = open(keyboard_device(), O_RDONLY | O_NONBLOCK);
    if (keyboard_fd < 0) {
        std::perror("[cp0] open evdev");
        keyboard_resume();
        return -1;
    }
    std::printf("[cp0] Opened evdev %s (no EVIOCGRAB; shared with child)\n", keyboard_device());
    std::fflush(stdout);

    const pid_t pid = fork();
    if (pid < 0) {
        close(keyboard_fd);
        keyboard_resume();
        return -1;
    }
    if (pid == 0) {
        close(keyboard_fd);
        setpgid(0, 0);
        if (keep_root)
            execlp("/bin/sh", "sh", "-c", command, static_cast<char *>(nullptr));
        else
            cp0_process_commands::exec_shell_as_configured_user(command);
        _exit(127);
    }

    setpgid(pid, pid);
    std::fprintf(stderr,
                 "[process] external app leader=%d pgid=%d subreaper=%d\n",
                 static_cast<int>(pid),
                 static_cast<int>(pid),
                 subreaper ? 1 : 0);

    auto esc_down_since = std::chrono::steady_clock::time_point{};
    auto term_start = std::chrono::steady_clock::time_point{};
    bool esc_down = false;
    bool raw_esc_down = false;
    bool term_sent = false;
    bool kill_sent = false;
    bool leader_reaped = false;
    int status = 0;

    while (true) {
        cp0_process_group::reap_available(pid, pid, status, leader_reaped);
        if (!cp0_process_group::exists(pid)) break;

        struct input_event event;
        while (read(keyboard_fd, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event))) {
            if (event.type == EV_KEY && event.code == KEY_ESC) {
                if (event.value == 1)
                    raw_esc_down = true;
                else if (event.value == 0)
                    raw_esc_down = false;
            }
        }

        const bool esc_now = raw_esc_down || (home_key_flag && *home_key_flag);
        if (esc_now && !esc_down) {
            esc_down = true;
            esc_down_since = std::chrono::steady_clock::now();
        } else if (!esc_now) {
            esc_down = false;
        }

        if (esc_down && !term_sent) {
            const auto held_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - esc_down_since)
                                     .count();
            if (held_ms >= 3000) {
                term_sent = true;
                term_start = std::chrono::steady_clock::now();
                std::fprintf(stderr, "[process] ESC timeout: SIGTERM pgid=%d\n", static_cast<int>(pid));
                killpg(pid, SIGTERM);
            }
        }
        if (term_sent && !kill_sent &&
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - term_start)
                    .count() >= 2) {
            kill_sent = true;
            std::fprintf(stderr, "[process] grace timeout: SIGKILL pgid=%d\n", static_cast<int>(pid));
            killpg(pid, SIGKILL);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    cp0_process_group::reap_available(pid, pid, status, leader_reaped);
    std::fprintf(stderr,
                 "[process] external app group drained pgid=%d leader_reaped=%d\n",
                 static_cast<int>(pid),
                 leader_reaped ? 1 : 0);
    close(keyboard_fd);
    keyboard_resume();
    std::printf("[cp0] Returned to launcher\n");
    std::fflush(stdout);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

} // namespace cp0_external_app_runner
