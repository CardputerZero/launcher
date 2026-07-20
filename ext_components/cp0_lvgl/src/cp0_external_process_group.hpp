#pragma once

#if !defined(_WIN32)

#include <cerrno>
#include <chrono>
#include <csignal>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace cp0_process_group {

inline bool enable_subreaper()
{
#if defined(__linux__)
    return prctl(PR_SET_CHILD_SUBREAPER, 1) == 0;
#else
    return false;
#endif
}

inline bool exists(pid_t pgid)
{
    if (pgid <= 0) return false;
    if (kill(-pgid, 0) == 0) return true;
    return errno == EPERM;
}

inline void reap_available(pid_t pgid, pid_t leader, int &leader_status,
                           bool &leader_reaped)
{
    if (pgid <= 0) return;
    for (;;) {
        int status = 0;
        pid_t reaped = waitpid(-pgid, &status, WNOHANG);
        if (reaped > 0) {
            if (reaped == leader) {
                leader_status = status;
                leader_reaped = true;
            }
            continue;
        }
        if (reaped < 0 && errno == EINTR) continue;
        break;
    }
}

inline bool exited_unreaped(pid_t pid)
{
    if (pid <= 0) return false;
    siginfo_t info{};
    if (waitid(P_PID, pid, &info, WEXITED | WNOHANG | WNOWAIT) < 0)
        return errno == ECHILD;
    return info.si_pid == pid;
}

inline bool defer_reap(pid_t pgid, pid_t leader,
                       std::chrono::milliseconds poll_interval =
                           std::chrono::milliseconds(100))
{
    try {
        std::thread([pgid, leader, poll_interval]() {
            int leader_status = 0;
            bool leader_reaped = false;
            while (exists(pgid)) {
                reap_available(pgid, leader, leader_status, leader_reaped);
                if (exists(pgid)) std::this_thread::sleep_for(poll_interval);
            }
            reap_available(pgid, leader, leader_status, leader_reaped);
        }).detach();
    } catch (...) {
        return false;
    }
    return true;
}

inline bool terminate_and_reap(pid_t pgid, pid_t leader,
                               std::chrono::milliseconds grace = std::chrono::seconds(2),
                               std::chrono::milliseconds poll_interval =
                                   std::chrono::milliseconds(10),
                               std::chrono::milliseconds kill_wait =
                                   std::chrono::seconds(2))
{
    if (pgid <= 0) return false;

    int leader_status = 0;
    bool leader_reaped = false;
    if (kill(-pgid, SIGTERM) < 0) {
        if (errno != ESRCH) return false;
        reap_available(pgid, leader, leader_status, leader_reaped);
        return true;
    }

    // Keep the leader as a zombie during the grace period. Reaping it could
    // allow its PID/PGID to be reused before the forced group kill.
    const auto term_deadline = std::chrono::steady_clock::now() + grace;
    while (exists(pgid) && !exited_unreaped(leader) &&
           std::chrono::steady_clock::now() < term_deadline)
        std::this_thread::sleep_for(poll_interval);

    if (exists(pgid) && kill(-pgid, SIGKILL) < 0 && errno != ESRCH)
        return false;

    const auto kill_deadline = std::chrono::steady_clock::now() + kill_wait;
    while (exists(pgid) && std::chrono::steady_clock::now() < kill_deadline) {
        reap_available(pgid, leader, leader_status, leader_reaped);
        if (exists(pgid)) std::this_thread::sleep_for(poll_interval);
    }
    reap_available(pgid, leader, leader_status, leader_reaped);
    if (!exists(pgid)) return leader_reaped;

    // A task in uninterruptible sleep cannot act on SIGKILL yet. Do not block
    // the caller indefinitely; retain ownership of this PGID until it exits.
    return defer_reap(pgid, leader);
}

} // namespace cp0_process_group

#endif
