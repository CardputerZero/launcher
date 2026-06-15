#include "cp0_lvgl_app.h"
#include "hal/hal_process.h"
#include "hal_lvgl_bsp.h"
#include "../cp0_app_internal_utils.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <fstream>
#include <grp.h>
#include <list>
#include <memory>
#include <pwd.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <utility>

#if !defined(_WIN32)
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if !defined(HAL_PLATFORM_SDL) && !defined(_WIN32)
#include <linux/input.h>
#endif

extern "C" {
    extern void keyboard_pause(void);
    extern void keyboard_resume(void);
}

extern "C" void __attribute__((weak)) keyboard_pause(void) {}
extern "C" void __attribute__((weak)) keyboard_resume(void) {}

class ProcessSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    void api_call(arg_t arg, callback_t callback)
    {
        const std::string cmd = arg.empty() ? "" : arg.front();
        if (cmd == "ExecBlocking") {
            const std::string exec_path = nth_arg(arg, 1);
            volatile int *home_key_flag = decode_flag_ptr(nth_arg(arg, 2));
            int keep_root = std::atoi(nth_arg(arg, 3).c_str());
            report(callback, exec_blocking(exec_path.c_str(), home_key_flag, keep_root), "");
        } else if (cmd == "Spawn") {
            const std::string exec_path = nth_arg(arg, 1);
            int keep_root = std::atoi(nth_arg(arg, 2).c_str());
            cp0_pid_t pid = spawn(exec_path.c_str(), keep_root);
            report(callback, pid < 0 ? -1 : 0, std::to_string(pid));
        } else if (cmd == "Stop") {
            stop(static_cast<cp0_pid_t>(std::atoi(nth_arg(arg, 1).c_str())));
            report(callback, 0, "");
        } else if (cmd == "CheckLock") {
            int holder_pid = 0;
            int ret = check_lock(nth_arg(arg, 1).c_str(), &holder_pid);
            report(callback, ret, std::to_string(holder_pid));
        } else if (cmd == "Kill") {
            int pid = std::atoi(nth_arg(arg, 1).c_str());
            int grace_ms = std::atoi(nth_arg(arg, 2).c_str());
            kill_process(pid, grace_ms);
            report(callback, 0, "");
        } else if (cmd == "RunArgv") {
            int background = std::atoi(nth_arg(arg, 1).c_str());
            std::vector<std::string> argv = args_from(arg, 2);
            report(callback, run_argv(argv, background), "");
        } else if (cmd == "CaptureArgv") {
            std::vector<std::string> argv = args_from(arg, 1);
            std::string output;
            int ret = capture_argv(argv, output);
            report(callback, ret, output);
        } else if (cmd == "Shutdown") {
            system_shutdown();
            report(callback, 0, "");
        } else if (cmd == "Reboot") {
            system_reboot();
            report(callback, 0, "");
        } else {
            report(callback, -1, "unknown process api command");
        }
    }

    int exec_blocking(const char *exec_path, volatile int *home_key_flag, int keep_root)
    {
#if defined(_WIN32)
        (void)exec_path;
        (void)home_key_flag;
        (void)keep_root;
        return -1;
#elif defined(HAL_PLATFORM_SDL)
        return exec_blocking_sdl(exec_path, home_key_flag, keep_root);
#else
        return exec_blocking_cp0(exec_path, home_key_flag, keep_root);
#endif
    }

    cp0_pid_t spawn(const char *exec_path, int keep_root)
    {
#if defined(_WIN32)
        (void)exec_path;
        (void)keep_root;
        return -1;
#else
        pid_t pid = fork();
        if (pid < 0)
            return -1;
        if (pid == 0) {
            setpgid(0, 0);
            if (keep_root)
                execlp("/bin/sh", "sh", "-c", exec_path, static_cast<char *>(nullptr));
            else
                exec_as_user(exec_path);
            _exit(127);
        }
        setpgid(pid, pid);
        return static_cast<cp0_pid_t>(pid);
#endif
    }

    void stop(cp0_pid_t pid)
    {
#if !defined(_WIN32)
        if (pid <= 0)
            return;
        killpg(static_cast<pid_t>(pid), SIGTERM);
        int status = 0;
        waitpid(static_cast<pid_t>(pid), &status, WNOHANG);
#else
        (void)pid;
#endif
    }

    int check_lock(const char *lock_path, int *holder_pid)
    {
        if (holder_pid)
            *holder_pid = 0;
#if defined(_WIN32)
        (void)lock_path;
        return 0;
#else
        if (!lock_path || !holder_pid)
            return -1;
        int fd = open(lock_path, O_CREAT | O_RDWR, 0666);
        if (fd < 0)
            return -1;
        struct flock fl;
        std::memset(&fl, 0, sizeof(fl));
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        if (fcntl(fd, F_GETLK, &fl) == -1) {
            close(fd);
            return -1;
        }
        close(fd);
        if (fl.l_type != F_UNLCK) {
            *holder_pid = fl.l_pid;
            return fl.l_pid;
        }
        return 0;
#endif
    }

    void kill_process(int pid, int grace_ms)
    {
#if !defined(_WIN32)
        if (pid <= 0)
            return;
        killpg(pid, SIGINT);
        auto start = std::chrono::steady_clock::now();
        while (true) {
            int status = 0;
            if (waitpid(pid, &status, WNOHANG) != 0)
                return;
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= grace_ms) {
                killpg(pid, SIGKILL);
                waitpid(pid, &status, 0);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
#else
        (void)pid;
        (void)grace_ms;
#endif
    }

    int run_argv(const std::vector<std::string> &argv, int background)
    {
#if defined(_WIN32)
        (void)argv;
        (void)background;
        return -1;
#else
        if (argv.empty() || argv[0].empty())
            return -EINVAL;

        pid_t pid = fork();
        if (pid < 0)
            return -errno;

        if (pid == 0) {
            if (background)
                redirect_to_devnull();
            auto raw = make_argv(argv);
            execvp(raw[0], raw.data());
            _exit(127);
        }

        if (background)
            return 0;

        int status = 0;
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR)
                return -errno;
        }
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        if (WIFSIGNALED(status))
            return 128 + WTERMSIG(status);
        return -1;
#endif
    }

    int capture_argv(const std::vector<std::string> &argv, std::string &output)
    {
        output.clear();
#if defined(_WIN32)
        (void)argv;
        return -1;
#else
        if (argv.empty() || argv[0].empty())
            return -EINVAL;

        int pipefd[2];
        if (pipe(pipefd) != 0)
            return -errno;

        pid_t pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            return -errno;
        }

        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDERR_FILENO);
                if (devnull > STDERR_FILENO)
                    close(devnull);
            }
            if (pipefd[1] > STDERR_FILENO)
                close(pipefd[1]);
            auto raw = make_argv(argv);
            execvp(raw[0], raw.data());
            _exit(127);
        }

        close(pipefd[1]);
        char buf[256];
        ssize_t n = 0;
        while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
            output.append(buf, static_cast<size_t>(n));
        close(pipefd[0]);

        int status = 0;
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR)
                return -errno;
        }
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
#endif
    }

    void system_shutdown()
    {
        std::printf("[SDL] shutdown requested (simulated no-op)\n");
    }

    void system_reboot()
    {
        std::printf("[SDL] reboot requested (simulated no-op)\n");
    }

private:
    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        for (size_t i = 0; i < index && it != arg.end(); ++i)
            ++it;
        return it == arg.end() ? std::string() : *it;
    }

    static std::vector<std::string> args_from(const arg_t &arg, size_t index)
    {
        std::vector<std::string> out;
        auto it = arg.begin();
        for (size_t i = 0; i < index && it != arg.end(); ++i)
            ++it;
        for (; it != arg.end(); ++it)
            out.push_back(*it);
        return out;
    }

    static volatile int *decode_flag_ptr(const std::string &text)
    {
        if (text.empty() || text == "0")
            return nullptr;
        uintptr_t raw = static_cast<uintptr_t>(std::strtoull(text.c_str(), nullptr, 10));
        return reinterpret_cast<volatile int *>(raw);
    }

    static bool is_nologin_shell(const char *shell)
    {
        if (!shell || !shell[0])
            return true;
        return std::strstr(shell, "nologin") != nullptr || std::strstr(shell, "/bin/false") != nullptr;
    }

    static std::string config_get_str(const char *key, const char *default_val)
    {
        std::string value = default_val ? default_val : "";
        cp0_signal_config_api({"GetStr", key ? std::string(key) : std::string(), value},
                              [&](int code, std::string data) {
                                  if (code == 0) value = std::move(data);
                              });
        return value;
    }

    static const char *get_run_user()
    {
        static thread_local std::string cfg;
        cfg = config_get_str("run_as_user", nullptr);
        if (!cfg.empty())
            return cfg.c_str();

        struct passwd *pwd;
        setpwent();
        while ((pwd = getpwent()) != nullptr) {
            if (pwd->pw_uid >= 1000 && pwd->pw_uid < 65534 && !is_nologin_shell(pwd->pw_shell)) {
                endpwent();
                return pwd->pw_name;
            }
        }
        endpwent();
        return "pi";
    }

    static void exec_as_user(const char *exec_path)
    {
#if defined(_WIN32)
        (void)exec_path;
#else
        const char *user = get_run_user();
        if (getuid() == 0 && std::strcmp(user, "root") != 0) {
            struct passwd *pw = getpwnam(user);
            if (pw) {
                initgroups(pw->pw_name, pw->pw_gid);
                setgid(pw->pw_gid);
                setuid(pw->pw_uid);
                setenv("HOME", pw->pw_dir, 1);
                setenv("USER", pw->pw_name, 1);
                setenv("LOGNAME", pw->pw_name, 1);
                setenv("SHELL", pw->pw_shell[0] ? pw->pw_shell : "/bin/bash", 1);
                chdir(pw->pw_dir);
            }
        }
        execlp("/bin/sh", "sh", "-c", exec_path, static_cast<char *>(nullptr));
#endif
    }

    static std::vector<char *> make_argv(const std::vector<std::string> &argv)
    {
        std::vector<char *> raw;
        raw.reserve(argv.size() + 1);
        for (const auto &arg : argv)
            raw.push_back(const_cast<char *>(arg.c_str()));
        raw.push_back(nullptr);
        return raw;
    }

    static void redirect_to_devnull()
    {
#if !defined(_WIN32)
        int fd = open("/dev/null", O_RDWR);
        if (fd < 0)
            return;
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO)
            close(fd);
#endif
    }

    int exec_blocking_sdl(const char *exec_path, volatile int *home_key_flag, int keep_root)
    {
#if defined(_WIN32)
        (void)exec_path;
        (void)home_key_flag;
        (void)keep_root;
        return -1;
#else
        (void)keep_root;
        pid_t pid = fork();
        if (pid < 0)
            return -1;
        if (pid == 0) {
            execlp("/bin/sh", "sh", "-c", exec_path, static_cast<char *>(nullptr));
            _exit(127);
        }
        int status = 0;
        int home_status = 0;
        std::chrono::steady_clock::time_point home_start;
        while (true) {
            int r = waitpid(pid, &status, WNOHANG);
            if (r > 0)
                break;
            if (r < 0) {
                status = 0;
                break;
            }

            if (home_key_flag) {
                if (home_status == 0 && *home_key_flag) {
                    home_status = 1;
                    home_start = std::chrono::steady_clock::now();
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
        if (home_key_flag)
            *home_key_flag = 0;
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
#endif
    }

    int exec_blocking_cp0(const char *exec_path, volatile int *home_key_flag, int keep_root)
    {
#if defined(_WIN32) || defined(HAL_PLATFORM_SDL)
        return exec_blocking_sdl(exec_path, home_key_flag, keep_root);
#else
        (void)home_key_flag;
        keyboard_pause();

        int evfd = open(get_kbd_device(), O_RDONLY | O_NONBLOCK);
        if (evfd < 0) {
            std::perror("[cp0] open evdev");
            keyboard_resume();
            return -1;
        }
        std::printf("[cp0] Opened evdev %s (no EVIOCGRAB; shared with child)\n", get_kbd_device());
        std::fflush(stdout);

        pid_t pid = fork();
        if (pid < 0) {
            close(evfd);
            keyboard_resume();
            return -1;
        }
        if (pid == 0) {
            close(evfd);
            setpgid(0, 0);
            if (keep_root)
                execlp("/bin/sh", "sh", "-c", exec_path, static_cast<char *>(nullptr));
            else
                exec_as_user(exec_path);
            _exit(127);
        }
        setpgid(pid, pid);

        auto esc_down_since = std::chrono::steady_clock::time_point{};
        bool esc_down = false;
        int status = 0;

        while (true) {
            int r = waitpid(pid, &status, WNOHANG);
            if (r > 0)
                break;
            if (r < 0) {
                status = -1;
                break;
            }

            struct input_event ev;
            while (read(evfd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
                if (ev.type == EV_KEY && ev.code == KEY_ESC) {
                    if (ev.value == 1) {
                        esc_down = true;
                        esc_down_since = std::chrono::steady_clock::now();
                    } else if (ev.value == 0) {
                        esc_down = false;
                    }
                }
            }

            if (esc_down) {
                auto held_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - esc_down_since).count();
                if (held_ms >= 3000) {
                    killpg(pid, SIGTERM);
                    auto t0 = std::chrono::steady_clock::now();
                    while (waitpid(pid, &status, WNOHANG) == 0) {
                        if (std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - t0).count() >= 3) {
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
        std::printf("[cp0] Returned to launcher\n");
        std::fflush(stdout);
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
#endif
    }

    static const char *get_kbd_device()
    {
        const char *env = std::getenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE");
        return env ? env : "/dev/input/by-path/platform-3f804000.i2c-event";
    }

public:
    static int api_simple(const arg_t &arg, std::string *data = nullptr)
    {
        int result = -1;
        cp0_signal_process_api(arg, [&](int code, std::string out) {
            result = code;
            if (data)
                *data = std::move(out);
        });
        return result;
    }
};

static bool contains_shell_meta(const char *s)
{
    if (!s)
        return true;
    static const char *kMeta = "|&;<>`$\\\n\r";
    return std::strpbrk(s, kMeta) != nullptr;
}

static std::string first_token(const char *exec)
{
    std::istringstream iss(exec ? exec : "");
    std::string token;
    iss >> token;
    return token;
}

static bool file_executable(const std::string &path)
{
#if defined(_WIN32)
    (void)path;
    return false;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(path.c_str(), X_OK) == 0;
#endif
}

extern "C" void init_process(void)
{
    auto process = std::make_shared<ProcessSystem>();
    cp0_signal_process_api.append([process](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        process->api_call(std::move(arg), std::move(callback));
    });
}

extern "C" int cp0_process_exec_blocking(const char *exec_path, volatile int *home_key_flag, int keep_root)
{
    return ProcessSystem::api_simple({"ExecBlocking", exec_path ? exec_path : "",
                                      std::to_string(reinterpret_cast<uintptr_t>(home_key_flag)),
                                      std::to_string(keep_root)});
}

extern "C" cp0_pid_t cp0_process_spawn(const char *exec_path, int keep_root)
{
    std::string data;
    int ret = ProcessSystem::api_simple({"Spawn", exec_path ? exec_path : "", std::to_string(keep_root)}, &data);
    return ret == 0 ? static_cast<cp0_pid_t>(std::atoi(data.c_str())) : -1;
}

extern "C" void cp0_process_stop(cp0_pid_t pid)
{
    ProcessSystem::api_simple({"Stop", std::to_string(pid)});
}

extern "C" int cp0_process_check_lock(const char *lock_path, int *holder_pid)
{
    std::string data;
    int ret = ProcessSystem::api_simple({"CheckLock", lock_path ? lock_path : ""}, &data);
    if (holder_pid)
        *holder_pid = std::atoi(data.c_str());
    return ret;
}

extern "C" void cp0_process_kill(int pid, int grace_ms)
{
    ProcessSystem::api_simple({"Kill", std::to_string(pid), std::to_string(grace_ms)});
}

extern "C" int cp0_process_run_argv(const char *const *argv, int background)
{
    std::list<std::string> args = {"RunArgv", std::to_string(background)};
    if (argv) {
        for (int i = 0; argv[i]; ++i)
            args.push_back(argv[i]);
    }
    return ProcessSystem::api_simple(args);
}

extern "C" int cp0_process_capture_argv(const char *const *argv, char *out, int out_size)
{
    if (out && out_size > 0)
        out[0] = '\0';
    std::list<std::string> args = {"CaptureArgv"};
    if (argv) {
        for (int i = 0; argv[i]; ++i)
            args.push_back(argv[i]);
    }
    std::string data;
    int ret = ProcessSystem::api_simple(args, &data);
    if (out && out_size > 0)
        cp0_copy_string(out, out_size, data);
    return ret;
}

extern "C" int cp0_file_read_first_line(const char *path, char *out, int out_size)
{
    if (out && out_size > 0)
        out[0] = '\0';
    if (!path || !out || out_size <= 0)
        return -1;
    std::ifstream file(path);
    if (!file.is_open())
        return -1;
    std::string line;
    if (!std::getline(file, line))
        return -1;
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    cp0_copy_string(out, out_size, line);
    return 0;
}

extern "C" int cp0_desktop_exec_is_safe(const char *exec, char *reason, int reason_size)
{
    auto fail = [reason, reason_size](const char *msg) {
        cp0_copy_cstr(reason, reason_size, msg ? msg : "unsafe Exec");
        return 0;
    };

    if (!exec || !exec[0])
        return fail("empty Exec");
    if (std::strlen(exec) > 512)
        return fail("Exec too long");
    if (contains_shell_meta(exec))
        return fail("Exec contains shell metacharacters");

    const std::string token = first_token(exec);
    if (token.empty())
        return fail("missing executable");

    if (token.find('/') != std::string::npos) {
        if (!file_executable(token))
            return fail("executable path is not executable");
        return 1;
    }

    static const char *kAllowedNames[] = {"bash", "python3", "vim", "vi", "nano", "sh"};
    if (std::find(std::begin(kAllowedNames), std::end(kAllowedNames), token) != std::end(kAllowedNames))
        return 1;

    return fail("executable name is not allowlisted");
}

extern "C" void cp0_system_shutdown(void)
{
    ProcessSystem::api_simple({"Shutdown"});
}

extern "C" void cp0_system_reboot(void)
{
    ProcessSystem::api_simple({"Reboot"});
}

extern "C" int hal_process_exec_blocking(const char *exec_path, volatile int *home_key_flag, int keep_root)
{
    return cp0_process_exec_blocking(exec_path, home_key_flag, keep_root);
}

extern "C" hal_pid_t hal_process_spawn(const char *exec_path, int keep_root)
{
    return cp0_process_spawn(exec_path, keep_root);
}

extern "C" void hal_process_stop(hal_pid_t pid)
{
    cp0_process_stop(pid);
}

extern "C" int hal_process_check_lock(const char *lock_path, int *holder_pid)
{
    return cp0_process_check_lock(lock_path, holder_pid);
}

extern "C" void hal_process_kill(int pid, int grace_ms)
{
    cp0_process_kill(pid, grace_ms);
}

extern "C" void hal_system_shutdown(void)
{
    cp0_system_shutdown();
}

extern "C" void hal_system_reboot(void)
{
    cp0_system_reboot();
}
