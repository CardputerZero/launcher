#pragma once

#if !defined(_WIN32)

#include <cerrno>
#include <csignal>
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

} // namespace cp0_process_group

#endif
