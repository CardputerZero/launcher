#include "../hal_process.h"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <linux/input.h>

extern "C" {
    extern void keyboard_pause(void);
    extern void keyboard_resume(void);
}

static const char *get_kbd_device()
{
    const char *env = getenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE");
    return env ? env : "/dev/input/by-path/platform-3f804000.i2c-event";
}

static const int ESC_HOLD_SEC = 3;

/* ------------------------------------------------------------------
 * Experiment:
 *  - Do NOT EVIOCGRAB the tca8418 evdev while the child is running.
 *  - Do NOT create a uinput mirror.
 *  - Child reads /dev/input/event* directly (same physical device);
 *    multiple readers each receive every input_event on an ungrabbed
 *    evdev, so both this loop (for ESC-hold detection) and the child
 *    can see the keys.
 *  - keyboard_pause() still suspends libinput so APPLauncher's LVGL
 *    keyboard thread doesn't react while the app is in the foreground.
 * ------------------------------------------------------------------ */
int hal_process_exec_blocking(const char *exec_path, volatile int *home_key_flag)
{
    (void)home_key_flag;

    keyboard_pause();

    int evfd = open(get_kbd_device(), O_RDONLY | O_NONBLOCK);
    if (evfd < 0) {
        perror("[hal] open evdev");
        keyboard_resume();
        return -1;
    }
    printf("[hal] Opened evdev %s (no EVIOCGRAB; shared with child)\n", get_kbd_device());
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) {
        close(evfd);
        keyboard_resume();
        return -1;
    }
    if (pid == 0) {
        close(evfd);
        /* Put the child (and everything it fork/execs) in its own
         * process group, so the launcher can kill the whole tree via
         * killpg() on long-press ESC. Otherwise sh often fork+waits an
         * inner sh that exec's the real binary, leaving the real
         * process as a grandchild that SIGTERM to `pid` never reaches. */
        setpgid(0, 0);
        execlp("/bin/sh", "sh", "-c", exec_path, (char *)NULL);
        _exit(127);
    }
    /* Also set it in the parent in case setpgid races the child. */
    setpgid(pid, pid);

    auto esc_down_since = std::chrono::steady_clock::time_point{};
    bool esc_down = false;
    int status = 0;

    while (true) {
        int r = waitpid(pid, &status, WNOHANG);
        if (r > 0) break;
        if (r < 0) { status = -1; break; }

        struct input_event ev;
        while (read(evfd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY) {
                const char *st = (ev.value == 1) ? "DOWN" :
                                 (ev.value == 0) ? "UP"   :
                                 (ev.value == 2) ? "REPEAT" : "???";
                printf("[HAL-EXT] evdev code=%u value=%d(%s) (shared, child reads too)\n",
                       ev.code, ev.value, st);
                fflush(stdout);
            }
            if (ev.type == EV_KEY && ev.code == KEY_ESC) {
                if (ev.value == 1) {
                    esc_down = true;
                    esc_down_since = std::chrono::steady_clock::now();
                    printf("[HAL-EXT] ESC DOWN\n");
                    fflush(stdout);
                } else if (ev.value == 0) {
                    esc_down = false;
                    printf("[HAL-EXT] ESC UP\n");
                    fflush(stdout);
                }
            }
        }

        if (esc_down) {
            auto held_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - esc_down_since).count();
            if (held_ms >= ESC_HOLD_SEC * 1000) {
                printf("[hal] ESC held %ldms, SIGTERM pgid %d\n",
                       (long)held_ms, pid);
                fflush(stdout);
                /* Kill the whole process group, not just pid, because
                 * sh -c may have fork'd an inner shell that exec'd the
                 * real binary as a grandchild. killpg reaches them all
                 * via the pgid we set with setpgid() above. */
                killpg(pid, SIGTERM);
                auto t0 = std::chrono::steady_clock::now();
                while (waitpid(pid, &status, WNOHANG) == 0) {
                    if (std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - t0).count() >= 3) {
                        printf("[hal] SIGKILL pgid %d\n", pid);
                        fflush(stdout);
                        killpg(pid, SIGKILL);
                        waitpid(pid, &status, 0);
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(evfd);

    keyboard_resume();

    printf("[hal] Returned to launcher\n");
    fflush(stdout);
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
    /* killpg: hal_process_spawn puts the child in its own pgid, so
     * SIGINT/SIGKILL here reaches grandchildren too (sh + exec'd
     * binary are typically both inside). */
    killpg(pid, SIGINT);
    auto start = std::chrono::steady_clock::now();
    while (true) {
        int status;
        if (waitpid(pid, &status, WNOHANG) != 0) return;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= grace_ms) {
            killpg(pid, SIGKILL);
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
        setpgid(0, 0);
        execlp("/bin/sh", "sh", "-c", exec_path, (char *)NULL);
        _exit(127);
    }
    setpgid(pid, pid);
    return (hal_pid_t)pid;
}

void hal_process_stop(hal_pid_t pid)
{
    if (pid <= 0) return;
    killpg((pid_t)pid, SIGTERM);
    int status;
    waitpid((pid_t)pid, &status, WNOHANG);
}

void hal_system_shutdown(void)
{
    printf("[HAL] shutdown\n");
    system("sudo shutdown -h now");
}

void hal_system_reboot(void)
{
    printf("[HAL] reboot\n");
    system("sudo reboot");
}
