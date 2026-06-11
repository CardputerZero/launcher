#include "hal/hal_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

void hal_audio_init(void) {}

void hal_audio_play(const char *path)
{
    if (!path || access(path, F_OK) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); close(devnull); }
        execlp("aplay", "aplay", "-q", path, (char *)NULL);
        execlp("mpv", "mpv", "--no-video", "--really-quiet", path, (char *)NULL);
        _exit(127);
    }
}

void hal_audio_play_sync(const char *path)
{
    if (!path || access(path, F_OK) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); close(devnull); }
        execlp("aplay", "aplay", "-q", path, (char *)NULL);
        execlp("mpv", "mpv", "--no-video", "--really-quiet", path, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) waitpid(pid, NULL, 0);
}

void hal_audio_stop(void) {}
void hal_audio_deinit(void) {}
