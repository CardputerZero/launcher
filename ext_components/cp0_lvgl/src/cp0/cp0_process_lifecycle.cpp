#include "cp0_process_lifecycle.hpp"

#include "../cp0_external_process_group.hpp"
#include "cp0_process_commands.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#if !defined(_WIN32)
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace cp0_process_lifecycle {

cp0_pid_t spawn(const char *command, bool keep_root)
{
#if defined(_WIN32)
    (void)command;
    (void)keep_root;
    return -1;
#else
    cp0_process_group::enable_subreaper();
    const pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        setpgid(0, 0);
        if (keep_root)
            execlp("/bin/sh", "sh", "-c", command, static_cast<char *>(nullptr));
        else
            cp0_process_commands::exec_shell_as_configured_user(command);
        _exit(127);
    }
    setpgid(pid, pid);
    return static_cast<cp0_pid_t>(pid);
#endif
}

void stop(cp0_pid_t pid)
{
#if defined(_WIN32)
    (void)pid;
#else
    if (pid <= 0) return;
    if (!cp0_process_group::terminate_and_reap(static_cast<pid_t>(pid),
                                               static_cast<pid_t>(pid))) {
        std::fprintf(stderr,
                     "[process] failed to stop and reap pgid=%d\n",
                     static_cast<int>(pid));
    }
#endif
}

int check_lock(const char *path, int *holder_pid)
{
    if (holder_pid) *holder_pid = 0;
#if defined(_WIN32)
    (void)path;
    return 0;
#else
    if (!path || !holder_pid) return -1;
    const int fd = open(path, O_CREAT | O_RDWR, 0666);
    if (fd < 0) return -1;

    struct flock lock;
    std::memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(fd, F_GETLK, &lock) == -1) {
        close(fd);
        return -1;
    }
    close(fd);
    if (lock.l_type == F_UNLCK) return 0;
    *holder_pid = lock.l_pid;
    return lock.l_pid;
#endif
}

void kill(int pid, int grace_ms)
{
#if defined(_WIN32)
    (void)pid;
    (void)grace_ms;
#else
    if (pid <= 0) return;
    killpg(pid, SIGINT);
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) != 0) return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        if (elapsed >= grace_ms) {
            killpg(pid, SIGKILL);
            waitpid(pid, &status, 0);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
#endif
}

} // namespace cp0_process_lifecycle
