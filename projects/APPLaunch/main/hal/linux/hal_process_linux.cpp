#include "../hal_process.h"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <thread>

int hal_process_exec_blocking(const char *exec_path, volatile int *home_key_flag)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execlp("/bin/sh", "sh", "-c", exec_path, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    int home_status = 0; /* 0=idle, 1=timing, 2=killing */
    std::chrono::steady_clock::time_point home_start;
    while (true) {
        int r = waitpid(pid, &status, WNOHANG);
        if (r > 0) break;
        if (r < 0) { status = 0; break; }

        if (home_key_flag) {
            if (home_status == 0) {
                /* 等待 home 键按下 */
                if (*home_key_flag) {
                    home_status = 1;
                    home_start = std::chrono::steady_clock::now();
                }
            } else if (home_status == 1) {
                if (*home_key_flag) {
                    /* 持续按住：检查是否已满5秒 */
                    auto elapsed = std::chrono::steady_clock::now() - home_start;
                    if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= 5) {
                        home_status = 2;
                        kill(pid, SIGINT);
                    }
                } else {
                    /* 松开了，重置计时 */
                    home_status = 0;
                }
            } else if (home_status == 2) {
                /* 已发 SIGINT，等待最多3秒后 SIGKILL */
                auto elapsed = std::chrono::steady_clock::now() - home_start;
                if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= 8) {
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);
                    break;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    /* 清零 home_key_flag，避免返回后残留状态影响 LVGL */
    if (home_key_flag)
        *home_key_flag = 0;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

int hal_process_check_lock(const char *lock_path, int *holder_pid)
{
    *holder_pid = 0;
    int fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    if (fd < 0) return -1;
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    if (fcntl(fd, F_GETLK, &fl) == -1) { close(fd); return -1; }
    close(fd);
    if (fl.l_type != F_UNLCK) {
        *holder_pid = fl.l_pid;
        return fl.l_pid;
    }
    return 0;
}

void hal_process_kill(int pid, int grace_ms)
{
    if (pid <= 0) return;
    kill(pid, SIGINT);
    auto start = std::chrono::steady_clock::now();
    while (true) {
        int status;
        if (waitpid(pid, &status, WNOHANG) != 0) return;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= grace_ms) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

hal_pid_t hal_process_spawn(const char *exec_path)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execlp("/bin/sh", "sh", "-c", exec_path, (char *)NULL);
        _exit(127);
    }
    return (hal_pid_t)pid;
}

void hal_process_stop(hal_pid_t pid)
{
    if (pid <= 0) return;
    kill((pid_t)pid, SIGTERM);
    int status;
    waitpid((pid_t)pid, &status, WNOHANG);
}
