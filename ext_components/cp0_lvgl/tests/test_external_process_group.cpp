#include "../src/cp0_external_process_group.hpp"

#include <cassert>
#include <chrono>
#include <csignal>
#include <cerrno>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>

int main()
{
    using namespace std::chrono;

    assert(cp0_process_group::enable_subreaper());

    pid_t leader = fork();
    assert(leader >= 0);
    if (leader == 0) {
        setpgid(0, 0);
        pid_t worker = fork();
        if (worker < 0) _exit(2);
        if (worker == 0) {
            std::this_thread::sleep_for(milliseconds(250));
            _exit(0);
        }
        _exit(0);
    }

    setpgid(leader, leader);
    int leader_status = 0;
    bool leader_reaped = false;
    const auto start = steady_clock::now();
    bool observed_leader_exit_with_live_group = false;

    while (cp0_process_group::exists(leader)) {
        cp0_process_group::reap_available(leader, leader, leader_status, leader_reaped);
        if (leader_reaped && cp0_process_group::exists(leader))
            observed_leader_exit_with_live_group = true;
        assert(steady_clock::now() - start < seconds(2));
        std::this_thread::sleep_for(milliseconds(10));
    }
    cp0_process_group::reap_available(leader, leader, leader_status, leader_reaped);

    assert(leader_reaped);
    assert(WIFEXITED(leader_status) && WEXITSTATUS(leader_status) == 0);
    assert(observed_leader_exit_with_live_group);
    assert(!cp0_process_group::terminate_and_reap(0, 0));

    int graceful_ready[2];
    assert(pipe(graceful_ready) == 0);
    leader = fork();
    assert(leader >= 0);
    if (leader == 0) {
        close(graceful_ready[0]);
        setpgid(0, 0);
        pid_t worker = fork();
        if (worker < 0) _exit(2);
        if (worker == 0) {
            const char value = 'x';
            if (write(graceful_ready[1], &value, 1) != 1) _exit(3);
            for (;;) pause();
        }
        for (;;) pause();
    }

    close(graceful_ready[1]);
    setpgid(leader, leader);
    char graceful_value = 0;
    assert(read(graceful_ready[0], &graceful_value, 1) == 1 && graceful_value == 'x');
    close(graceful_ready[0]);
    assert(cp0_process_group::terminate_and_reap(
        leader, leader, milliseconds(250), milliseconds(5)));
    errno = 0;
    assert(waitpid(-leader, nullptr, WNOHANG) == -1 && errno == ECHILD);
    assert(!cp0_process_group::exists(leader));

    int mixed_ready[2];
    assert(pipe(mixed_ready) == 0);
    leader = fork();
    assert(leader >= 0);
    if (leader == 0) {
        close(mixed_ready[0]);
        setpgid(0, 0);
        pid_t worker = fork();
        if (worker < 0) _exit(2);
        if (worker == 0) {
            signal(SIGTERM, SIG_IGN);
            const char value = 'x';
            if (write(mixed_ready[1], &value, 1) != 1) _exit(3);
            for (;;) pause();
        }
        for (;;) pause();
    }

    close(mixed_ready[1]);
    setpgid(leader, leader);
    char mixed_value = 0;
    assert(read(mixed_ready[0], &mixed_value, 1) == 1 && mixed_value == 'x');
    close(mixed_ready[0]);
    assert(cp0_process_group::terminate_and_reap(
        leader, leader, milliseconds(50), milliseconds(5)));
    errno = 0;
    assert(waitpid(-leader, nullptr, WNOHANG) == -1 && errno == ECHILD);
    assert(!cp0_process_group::exists(leader));

    int ready[2];
    assert(pipe(ready) == 0);
    leader = fork();
    assert(leader >= 0);
    if (leader == 0) {
        close(ready[0]);
        setpgid(0, 0);
        signal(SIGTERM, SIG_IGN);
        pid_t worker = fork();
        if (worker < 0) _exit(2);
        if (worker == 0) {
            signal(SIGTERM, SIG_IGN);
            const char value = 'x';
            if (write(ready[1], &value, 1) != 1) _exit(3);
            for (;;) pause();
        }
        for (;;) pause();
    }

    close(ready[1]);
    setpgid(leader, leader);
    char value = 0;
    assert(read(ready[0], &value, 1) == 1 && value == 'x');
    close(ready[0]);
    assert(cp0_process_group::terminate_and_reap(
        leader, leader, milliseconds(50), milliseconds(5)));
    errno = 0;
    assert(waitpid(-leader, nullptr, WNOHANG) == -1 && errno == ECHILD);
    assert(!cp0_process_group::exists(leader));

    int deferred_ready[2];
    assert(pipe(deferred_ready) == 0);
    leader = fork();
    assert(leader >= 0);
    if (leader == 0) {
        close(deferred_ready[0]);
        setpgid(0, 0);
        signal(SIGTERM, SIG_IGN);
        const char value = 'x';
        if (write(deferred_ready[1], &value, 1) != 1) _exit(3);
        for (;;) pause();
    }

    close(deferred_ready[1]);
    setpgid(leader, leader);
    char deferred_value = 0;
    assert(read(deferred_ready[0], &deferred_value, 1) == 1 && deferred_value == 'x');
    close(deferred_ready[0]);
    assert(cp0_process_group::defer_reap(leader, leader, milliseconds(5)));
    std::this_thread::sleep_for(milliseconds(20));
    assert(cp0_process_group::exists(leader));
    assert(kill(-leader, SIGKILL) == 0);
    const auto deferred_deadline = steady_clock::now() + seconds(1);
    while (cp0_process_group::exists(leader) && steady_clock::now() < deferred_deadline)
        std::this_thread::sleep_for(milliseconds(5));
    assert(!cp0_process_group::exists(leader));
    errno = 0;
    assert(waitpid(-leader, nullptr, WNOHANG) == -1 && errno == ECHILD);
}
