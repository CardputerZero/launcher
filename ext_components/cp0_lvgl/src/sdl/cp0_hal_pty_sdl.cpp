#include "hal/hal_pty.h"

#if defined(_WIN32) || defined(__EMSCRIPTEN__)
hal_pty_t hal_pty_open(const char *cmd, const char *const *args, int cols, int rows)
{ (void)cmd; (void)args; (void)cols; (void)rows; return NULL; }
int  hal_pty_read(hal_pty_t p, char *b, size_t s) { (void)p; (void)b; (void)s; return -1; }
int  hal_pty_write(hal_pty_t p, const char *b, size_t l) { (void)p; (void)b; (void)l; return -1; }
int  hal_pty_check_child(hal_pty_t p, int *e) { (void)p; (void)e; return -1; }
void hal_pty_close(hal_pty_t p) { (void)p; }

#else
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

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
        if (args) {
            execvp(cmd, (char *const *)args);
        } else {
            execlp(cmd, cmd, (char *)NULL);
        }
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

#endif
