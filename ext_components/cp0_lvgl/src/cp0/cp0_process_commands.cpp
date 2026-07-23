#include "cp0_process_commands.hpp"

#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../cp0_sync_signal.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <utility>

#if !defined(_WIN32)
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace cp0_process_commands {
namespace {

std::vector<char *> make_argv(const std::vector<std::string> &argv)
{
    std::vector<char *> raw;
    raw.reserve(argv.size() + 1);
    for (const auto &arg : argv)
        raw.push_back(const_cast<char *>(arg.c_str()));
    raw.push_back(nullptr);
    return raw;
}

#if !defined(_WIN32)
int wait_for_child(pid_t pid, bool report_signal = true)
{
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -errno;
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (report_signal && WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return -1;
}

bool write_all(int fd, const char *data, size_t size)
{
    size_t offset = 0;
    while (offset < size) {
        const ssize_t written = ::write(fd, data + offset, size - offset);
        if (written > 0) {
            offset += static_cast<size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        return false;
    }
    return true;
}

void redirect_to_devnull()
{
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
        return;
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO)
        close(fd);
}

bool is_nologin_shell(const char *shell)
{
    if (!shell || !shell[0])
        return true;
    return std::strstr(shell, "nologin") != nullptr ||
           std::strstr(shell, "/bin/false") != nullptr;
}

std::string configured_run_user()
{
    std::string user;
    cp0::signal::invoke_noexcept([&] { cp0_signal_config_api({"GetStr", "run_as_user", ""},
                          [&](int code, std::string data) {
                              if (code == 0)
                                  user = std::move(data);
                          }); });
    if (!user.empty())
        return user;

    struct passwd *pwd;
    setpwent();
    while ((pwd = getpwent()) != nullptr) {
        if (pwd->pw_uid >= 1000 && pwd->pw_uid < 65534 &&
            !is_nologin_shell(pwd->pw_shell)) {
            user = pwd->pw_name;
            break;
        }
    }
    endpwent();
    return user.empty() ? "pi" : user;
}
#endif

} // namespace

int run_argv(const std::vector<std::string> &argv, bool background)
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
    return background ? 0 : wait_for_child(pid);
#endif
}

int run_sudo(const std::string &password, const std::vector<std::string> &argv)
{
#if defined(_WIN32)
    (void)password;
    (void)argv;
    return -1;
#else
    if (argv.empty() || argv[0].empty())
        return -EINVAL;

    std::vector<std::string> sudo_argv = {"sudo", "-k", "-S", "--"};
    sudo_argv.insert(sudo_argv.end(), argv.begin(), argv.end());

    int stdin_pipe[2];
    if (pipe(stdin_pipe) != 0)
        return -errno;
    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return -errno;
    }
    if (pid == 0) {
        close(stdin_pipe[1]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        if (stdin_pipe[0] > STDIN_FILENO)
            close(stdin_pipe[0]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO)
                close(devnull);
        }
        auto raw = make_argv(sudo_argv);
        execvp(raw[0], raw.data());
        _exit(127);
    }

    close(stdin_pipe[0]);
    const std::string password_line = password + "\n";
    const bool password_written =
        write_all(stdin_pipe[1], password_line.data(), password_line.size());
    const int write_error = password_written ? 0 : errno;
    close(stdin_pipe[1]);
    const int child_result = wait_for_child(pid);
    return password_written ? child_result : -(write_error ? write_error : EIO);
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

    int output_pipe[2];
    if (pipe(output_pipe) != 0)
        return -errno;
    pid_t pid = fork();
    if (pid < 0) {
        close(output_pipe[0]);
        close(output_pipe[1]);
        return -errno;
    }
    if (pid == 0) {
        close(output_pipe[0]);
        dup2(output_pipe[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO)
                close(devnull);
        }
        if (output_pipe[1] > STDERR_FILENO)
            close(output_pipe[1]);
        auto raw = make_argv(argv);
        execvp(raw[0], raw.data());
        _exit(127);
    }

    close(output_pipe[1]);
    char buffer[256];
    ssize_t count;
    while ((count = read(output_pipe[0], buffer, sizeof(buffer))) > 0)
        output.append(buffer, static_cast<size_t>(count));
    close(output_pipe[0]);
    return wait_for_child(pid, false);
#endif
}

void exec_shell_as_configured_user(const char *command)
{
#if defined(_WIN32)
    (void)command;
#else
    if (!command || !command[0])
        _exit(127);
    const std::string user = configured_run_user();
    if (getuid() == 0 && user != "root") {
        struct passwd *pwd = getpwnam(user.c_str());
        if (!pwd || initgroups(pwd->pw_name, pwd->pw_gid) != 0 ||
            setgid(pwd->pw_gid) != 0 || setuid(pwd->pw_uid) != 0 ||
            setenv("HOME", pwd->pw_dir, 1) != 0 ||
            setenv("USER", pwd->pw_name, 1) != 0 ||
            setenv("LOGNAME", pwd->pw_name, 1) != 0 ||
            setenv("SHELL", pwd->pw_shell[0] ? pwd->pw_shell : "/bin/bash", 1) != 0 ||
            chdir(pwd->pw_dir) != 0)
            _exit(126);
    }
    execlp("/bin/sh", "sh", "-c", command, static_cast<char *>(nullptr));
#endif
}

} // namespace cp0_process_commands
