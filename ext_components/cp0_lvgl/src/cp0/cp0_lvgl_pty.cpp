#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <grp.h>
#include <iterator>
#include <list>
#include <memory>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <pty.h>
#endif

namespace {

typedef void *pty_handle_t;

struct cp0_pty_handle {
    int master_fd;
    pid_t child_pid;
};

class PtySystem {
public:
    typedef std::function<void(int, std::string)> callback_t;
    typedef std::list<std::string> arg_t;

    pty_handle_t open(const char *cmd, const char *const *args, int cols, int rows)
    {
#if defined(__linux__)
        if (!cmd || !cmd[0]) return NULL;

        int master_fd = -1;
        struct winsize ws = {};
        ws.ws_col = cols;
        ws.ws_row = rows;
        std::string run_as_user = config_get_str("run_as_user", "");

        pid_t pid = forkpty(&master_fd, NULL, NULL, &ws);
        if (pid < 0) return NULL;

        if (pid == 0) {
            setenv("TERM", "vt100", 1);
            drop_root_user(run_as_user);

            if (args)
                execvp(cmd, const_cast<char *const *>(args));
            else
                execlp(cmd, cmd, static_cast<char *>(NULL));
            _exit(127);
        }

        int flags = fcntl(master_fd, F_GETFL);
        if (flags >= 0) fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

        cp0_pty_handle *pty = static_cast<cp0_pty_handle *>(std::malloc(sizeof(cp0_pty_handle)));
        if (!pty) {
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            ::close(master_fd);
            return NULL;
        }
        pty->master_fd = master_fd;
        pty->child_pid = pid;
        return pty;
#else
        (void)cmd;
        (void)args;
        (void)cols;
        (void)rows;
        return NULL;
#endif
    }

    int read(pty_handle_t pty, char *buf, size_t buf_size)
    {
#if defined(__linux__)
        if (!pty || !buf || buf_size == 0) return -1;
        cp0_pty_handle *h = static_cast<cp0_pty_handle *>(pty);
        ssize_t n = ::read(h->master_fd, buf, buf_size);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        return static_cast<int>(n);
#else
        (void)pty;
        (void)buf;
        (void)buf_size;
        return -1;
#endif
    }

    int write(pty_handle_t pty, const char *buf, size_t len)
    {
#if defined(__linux__)
        if (!pty || !buf) return -1;
        cp0_pty_handle *h = static_cast<cp0_pty_handle *>(pty);
        return static_cast<int>(::write(h->master_fd, buf, len));
#else
        (void)pty;
        (void)buf;
        (void)len;
        return -1;
#endif
    }

    int check_child(pty_handle_t pty, int *exit_status)
    {
#if defined(__linux__)
        if (!pty) return -1;
        cp0_pty_handle *h = static_cast<cp0_pty_handle *>(pty);
        int status = 0;
        pid_t r = waitpid(h->child_pid, &status, WNOHANG);
        if (r == 0) return 0;
        if (r > 0) {
            if (exit_status) *exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            return 1;
        }
        return -1;
#else
        (void)pty;
        (void)exit_status;
        return -1;
#endif
    }

    void close(pty_handle_t pty)
    {
#if defined(__linux__)
        if (!pty) return;
        cp0_pty_handle *h = static_cast<cp0_pty_handle *>(pty);
        kill(h->child_pid, SIGKILL);
        waitpid(h->child_pid, NULL, 0);
        ::close(h->master_fd);
        std::free(h);
#else
        (void)pty;
#endif
    }

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty()) {
            report(callback, -1, "empty pty api\n");
            return;
        }

        const std::string cmd = arg.front();
        if (cmd == "Open") {
            api_open(arg, callback);
        } else if (cmd == "Read") {
            api_read(arg, callback);
        } else if (cmd == "Write") {
            api_write(arg, callback);
        } else if (cmd == "CheckChild") {
            api_check_child(arg, callback);
        } else if (cmd == "Close") {
            pty_handle_t pty = parse_handle(nth_arg(arg, 1));
            close(pty);
            report(callback, 0, "");
        } else {
            report(callback, -1, "unknown pty api: " + cmd + "\n");
        }
    }

private:
    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback) callback(code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        if (index >= arg.size()) return "";
        auto it = arg.begin();
        std::advance(it, index);
        return *it;
    }

    static std::string handle_to_string(pty_handle_t pty)
    {
        std::ostringstream os;
        os << reinterpret_cast<uintptr_t>(pty);
        return os.str();
    }

    static pty_handle_t parse_handle(const std::string &value)
    {
        if (value.empty()) return NULL;
        char *end = NULL;
        uintptr_t raw = static_cast<uintptr_t>(std::strtoull(value.c_str(), &end, 0));
        if (!end || *end != '\0') return NULL;
        return reinterpret_cast<pty_handle_t>(raw);
    }

    static int parse_int(const std::string &value, int fallback)
    {
        if (value.empty()) return fallback;
        char *end = NULL;
        long parsed = std::strtol(value.c_str(), &end, 10);
        return (end && *end == '\0') ? static_cast<int>(parsed) : fallback;
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

    static void drop_root_user(const std::string &configured_user)
    {
#if defined(__linux__)
        if (getuid() != 0) return;

        const char *username = configured_user.empty() ? NULL : configured_user.c_str();
        if (!username) {
            struct passwd *p = NULL;
            setpwent();
            while ((p = getpwent()) != NULL) {
                if (p->pw_uid >= 1000 && p->pw_uid < 65534 &&
                    p->pw_shell && p->pw_shell[0] &&
                    !std::strstr(p->pw_shell, "nologin") &&
                    !std::strstr(p->pw_shell, "/bin/false")) {
                    username = p->pw_name;
                    break;
                }
            }
            endpwent();
        }
        if (!username) username = "pi";

        struct passwd *pw = getpwnam(username);
        if (pw && std::strcmp(username, "root") != 0) {
            initgroups(pw->pw_name, pw->pw_gid);
            setgid(pw->pw_gid);
            setuid(pw->pw_uid);
            setenv("HOME", pw->pw_dir, 1);
            setenv("USER", pw->pw_name, 1);
            setenv("LOGNAME", pw->pw_name, 1);
            setenv("SHELL", pw->pw_shell[0] ? pw->pw_shell : "/bin/bash", 1);
            chdir(pw->pw_dir);
        }
#endif
    }

    void api_open(const arg_t &arg, callback_t callback)
    {
        std::string exec = nth_arg(arg, 1);
        int cols = parse_int(nth_arg(arg, 2), 80);
        int rows = parse_int(nth_arg(arg, 3), 24);
        if (exec.empty()) {
            report(callback, -1, "empty pty command\n");
            return;
        }

        std::vector<std::string> argv_storage;
        for (auto it = arg.begin(); it != arg.end(); ++it) {
            size_t index = static_cast<size_t>(std::distance(arg.begin(), it));
            if (index >= 4) argv_storage.push_back(*it);
        }

        std::vector<const char *> argv;
        if (!argv_storage.empty()) {
            for (const std::string &item : argv_storage) argv.push_back(item.c_str());
            argv.push_back(NULL);
        }

        pty_handle_t pty = open(exec.c_str(), argv.empty() ? NULL : argv.data(), cols, rows);
        if (!pty) {
            report(callback, -1, "open pty failed\n");
            return;
        }
        report(callback, 0, handle_to_string(pty));
    }

    void api_read(const arg_t &arg, callback_t callback)
    {
        pty_handle_t pty = parse_handle(nth_arg(arg, 1));
        int max_len = parse_int(nth_arg(arg, 2), 4096);
        if (max_len <= 0) max_len = 4096;

        std::string out(static_cast<size_t>(max_len), '\0');
        int n = read(pty, &out[0], out.size());
        if (n < 0) {
            report(callback, -1, "read pty failed\n");
            return;
        }
        out.resize(static_cast<size_t>(n));
        report(callback, n, out);
    }

    void api_write(const arg_t &arg, callback_t callback)
    {
        pty_handle_t pty = parse_handle(nth_arg(arg, 1));
        std::string data = nth_arg(arg, 2);
        int n = write(pty, data.c_str(), data.size());
        report(callback, n, n < 0 ? "write pty failed\n" : "");
    }

    void api_check_child(const arg_t &arg, callback_t callback)
    {
        pty_handle_t pty = parse_handle(nth_arg(arg, 1));
        int status = 0;
        int ret = check_child(pty, &status);
        report(callback, ret, std::to_string(status));
    }
};

} // namespace

extern "C" void init_pty(void)
{
    std::shared_ptr<PtySystem> pty = std::make_shared<PtySystem>();
    cp0_signal_pty_api.append([pty](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        pty->api_call(std::move(arg), std::move(callback));
    });
}
