#include "../hal_process.h"
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>

int hal_process_exec_blocking(const char *exec_path, volatile int *home_key_flag)
{
    (void)home_key_flag;
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s", exec_path);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
}

int hal_process_check_lock(const char *lock_path, int *holder_pid)
{
    (void)lock_path;
    *holder_pid = 0;
    return 0;
}

void hal_process_kill(int pid, int grace_ms)
{
    (void)pid; (void)grace_ms;
}

hal_pid_t hal_process_spawn(const char *exec_path)
{
    (void)exec_path;
    return -1;
}

void hal_process_stop(hal_pid_t pid)
{
    (void)pid;
}

void hal_system_shutdown(void)
{
    printf("[HAL] shutdown (emulator exit)\n");
    exit(0);
}

void hal_system_reboot(void)
{
    printf("[HAL] reboot (emulator exit)\n");
    exit(0);
}

#else
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
                if (*home_key_flag) {
                    home_status = 1;
                    home_start = std::chrono::steady_clock::now();
                }
            } else if (home_status == 1) {
                if (*home_key_flag) {
                    auto elapsed = std::chrono::steady_clock::now() - home_start;
                    if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= 5) {
                        home_status = 2;
                        kill(pid, SIGINT);
                    }
                } else {
                    home_status = 0;
                }
            } else if (home_status == 2) {
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

void hal_system_shutdown(void)
{
    printf("[HAL] shutdown (emulator exit)\n");
    exit(0);
}

void hal_system_reboot(void)
{
    printf("[HAL] reboot (emulator exit)\n");
    exit(0);
}

#endif
