/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_external_app_runner.hpp"

#include "cp0_esc_exit_policy.hpp"
#include "cp0_esc_state.h"
#include "../cp0_external_process_group.hpp"
#include "cp0_process_commands.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <system_error>
#include <thread>

#if !defined(_WIN32)
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern "C" void __attribute__((weak)) keyboard_pause(void) {}
extern "C" void __attribute__((weak)) keyboard_resume(void) {}
extern "C" void __attribute__((weak)) ui_external_esc_hint(int visible) { (void)visible; }

namespace cp0_external_app_runner {
namespace {

const char *keyboard_device()
{
    const char *configured = std::getenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE");
    return configured ? configured : "/dev/input/by-path/platform-3f804000.i2c-event";
}

std::uint64_t monotonic_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace

int run(const char *command, bool keep_root)
{
#if defined(_WIN32)
    (void)command;
    (void)keep_root;
    return -1;
#else
    keyboard_pause();
    const bool subreaper = cp0_process_group::enable_subreaper();

    int keyboard_fd = open(keyboard_device(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
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

    cp0_esc_exit_policy::StateMachine esc_policy;
    // A shell may exit before its descendants. Watch the entire process group;
    // WNOWAIT leaves reaping, exit status and group signalling on this thread.
    int child_event = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    std::thread child_waiter;
    bool leader_reaped = false;
    int status = 0;

    while (true) {
        cp0_process_group::reap_available(pid, pid, status, leader_reaped);
        if (!cp0_process_group::exists(pid)) break;

        if (child_event >= 0 && !child_waiter.joinable()) {
            try {
                child_waiter = std::thread([pid, child_event] {
                    siginfo_t info{};
                    int rc;
                    do {
                        rc = waitid(P_PGID, pid, &info, WEXITED | WNOWAIT);
                    } while (rc < 0 && errno == EINTR);
                    while (eventfd_write(child_event, rc == 0 ? 1 : 2) < 0 && errno == EINTR) {}
                });
            } catch (const std::system_error &) {
                close(child_event);
                child_event = -1;
            }
        }

        struct input_event event;
        while (keyboard_fd >= 0 && read(keyboard_fd, &event, sizeof(event)) == sizeof(event)) {
            if (event.type == EV_KEY && event.code == KEY_ESC) {
                if (event.value == 1)
                    cp0_esc_state_write(1);
                else if (event.value == 0)
                    cp0_esc_state_write(0);
            }
        }

        const bool esc_now = cp0_esc_state_read() != 0;
        const auto decision = esc_policy.update(monotonic_ms(), esc_now);
        if (decision.show_hint) ui_external_esc_hint(1);
        if (decision.hide_hint) ui_external_esc_hint(0);
        if (decision.send_terminate) {
            std::fprintf(stderr, "[process] ESC timeout: SIGTERM pgid=%d\n", static_cast<int>(pid));
            killpg(pid, SIGTERM);
        }
        if (decision.send_kill) {
            std::fprintf(stderr, "[process] grace timeout: SIGKILL pgid=%d\n", static_cast<int>(pid));
            killpg(pid, SIGKILL);
        }
        int timeout = esc_policy.timeout_ms(monotonic_ms());
        // A non-waitable descendant (e.g. if subreaper setup failed) has no
        // child-exit event. Bound only that degraded path.
        if (child_event < 0 && (timeout < 0 || timeout > 100)) timeout = 100;
        pollfd fds[] = {{keyboard_fd, POLLIN, 0}, {child_event, POLLIN, 0}};
        if (poll(fds, 2, timeout) < 0) {
            if (errno == EINTR) continue;
            std::perror("[cp0] poll external app");
            continue;
        }
        if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            close(keyboard_fd);
            keyboard_fd = -1;
            cp0_esc_state_reset();
        }
        if (fds[1].revents & POLLIN) {
            child_waiter.join();
            eventfd_t result;
            if (eventfd_read(child_event, &result) < 0 || result != 1) {
                close(child_event);
                child_event = -1;
            }
        }
    }

    if (child_waiter.joinable()) child_waiter.join();
    if (child_event >= 0) close(child_event);
    if (esc_policy.finish().hide_hint) ui_external_esc_hint(0);
    cp0_process_group::reap_available(pid, pid, status, leader_reaped);
    std::fprintf(stderr,
                 "[process] external app group drained pgid=%d leader_reaped=%d\n",
                 static_cast<int>(pid),
                 leader_reaped ? 1 : 0);
    if (keyboard_fd >= 0) close(keyboard_fd);
    keyboard_resume();
    cp0_esc_state_reset();
    std::printf("[cp0] Returned to launcher\n");
    std::fflush(stdout);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

} // namespace cp0_external_app_runner
