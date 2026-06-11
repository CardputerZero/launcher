#include "hal/hal_pty.h"
#include "hal/hal_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <pty.h>
#include <pwd.h>
#include <grp.h>

struct hal_pty {
    int   master_fd;
    pid_t child_pid;
};

hal_pty_t hal_pty_open(const char *cmd, const char *const *args,
                       int cols, int rows)
{
    int master_fd;
    pid_t pid;
    struct winsize ws = {};
    ws.ws_col = cols;
    ws.ws_row = rows;

    pid = forkpty(&master_fd, NULL, NULL, &ws);
    if (pid < 0) return NULL;

    if (pid == 0) {
        setenv("TERM", "vt100", 1);

        // Drop to regular user if running as root
        if (getuid() == 0) {
            const char *cfg_user = hal_config_get_str("run_as_user", NULL);
            const char *username = NULL;
            if (cfg_user && cfg_user[0]) {
                username = cfg_user;
            } else {
                struct passwd *p;
                setpwent();
                while ((p = getpwent()) != NULL) {
                    if (p->pw_uid >= 1000 && p->pw_uid < 65534 &&
                        p->pw_shell && p->pw_shell[0] &&
                        !strstr(p->pw_shell, "nologin") &&
                        !strstr(p->pw_shell, "/bin/false")) {
                        username = p->pw_name;
                        break;
                    }
                }
                endpwent();
            }
            if (!username) username = "pi";

            struct passwd *pw = getpwnam(username);
            if (pw && strcmp(username, "root") != 0) {
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

        if (args)
            execvp(cmd, (char *const *)args);
        else
            execlp(cmd, cmd, (char *)NULL);
        _exit(127);
    }

    int flags = fcntl(master_fd, F_GETFL);
    fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

    struct hal_pty *pty = (struct hal_pty *)malloc(sizeof(struct hal_pty));
    pty->master_fd = master_fd;
    pty->child_pid = pid;
    return pty;
}

int hal_pty_read(hal_pty_t pty, char *buf, size_t buf_size)
{
    if (!pty) return -1;
    ssize_t n = read(pty->master_fd, buf, buf_size);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    return (int)n;
}

int hal_pty_write(hal_pty_t pty, const char *buf, size_t len)
{
    if (!pty) return -1;
    return (int)write(pty->master_fd, buf, len);
}

int hal_pty_check_child(hal_pty_t pty, int *exit_status)
{
    if (!pty) return -1;
    int status;
    pid_t r = waitpid(pty->child_pid, &status, WNOHANG);
    if (r == 0) return 0;
    if (r > 0) {
        if (exit_status) *exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return 1;
    }
    return -1;
}

void hal_pty_close(hal_pty_t pty)
{
    if (!pty) return;
    kill(pty->child_pid, SIGKILL);
    waitpid(pty->child_pid, NULL, 0);
    close(pty->master_fd);
    free(pty);
}
