#include "hal/hal_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

/* Subprocess-based audio HAL.
 *
 * Matches exactly what the audio player (ui_app_music.hpp) does —
 * a single fork + execlp("sh", "-c", "mpg123 -q '<file>'") — so env
 * (HOME, XDG_RUNTIME_DIR, PULSE_SERVER, etc.) is preserved and the
 * process talks to the same pipewire server the audio player uses.
 *
 * Zombie prevention: set SIGCHLD → SIG_IGN at process start so orphan
 * children are reaped automatically by the kernel. No waitpid in the
 * hot path — UI stays responsive. */

static int g_installed_sigchld = 0;
static void ensure_sigchld_autoreap(void)
{
    if (g_installed_sigchld) return;
    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = SA_NOCLDWAIT; /* kernel auto-reaps */
    sigaction(SIGCHLD, &sa, NULL);
    g_installed_sigchld = 1;
}

static int clamp_vol(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

/* Escape single-quotes in a path for inclusion inside a '...' string. */
static void sh_quote(const char *in, char *out, size_t out_sz)
{
    size_t o = 0;
    if (out_sz == 0) return;
    for (size_t i = 0; in[i] && o + 4 < out_sz; i++) {
        if (in[i] == '\'') {
            /* ' -> '\'' */
            if (o + 4 >= out_sz) break;
            out[o++] = '\''; out[o++] = '\\'; out[o++] = '\''; out[o++] = '\'';
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = 0;
}

/* Fire-and-forget: same pattern as hal_process_spawn + music player. */
static pid_t spawn_player(const char *path, int volume_pct, int wait)
{
    int v = clamp_vol(volume_pct);
    int scale = (int)((long)v * 32768 / 100);
    if (scale < 0) scale = 0;
    if (scale > 32768) scale = 32768;

    char quoted[512];
    sh_quote(path, quoted, sizeof(quoted));

    char cmd[1024];
    /* mpg123 works for both mp3 and wav; `-f <scale>` controls volume.
     * Shell-level `||` falls back to ffplay if mpg123 is absent.
     * stderr → /dev/null so transient "resource busy" races don't spam
     * the journal; mpg123 exit status is enough for diagnosis. */
    snprintf(cmd, sizeof(cmd),
             "mpg123 -q -f %d '%s' 2>/dev/null "
             "|| ffplay -nodisp -autoexit -loglevel quiet '%s' 2>/dev/null "
             "|| aplay -q '%s' 2>/dev/null",
             scale, quoted, quoted, quoted);

    ensure_sigchld_autoreap();

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "[HAL-audio] fork failed: %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        setpgid(0, 0);
        execlp("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    setpgid(pid, pid);
    if (wait) {
        int status = 0;
        /* Re-enable default SIGCHLD temporarily so waitpid works. */
        signal(SIGCHLD, SIG_DFL);
        waitpid(pid, &status, 0);
        /* Restore auto-reap for subsequent async calls. */
        g_installed_sigchld = 0;
        ensure_sigchld_autoreap();
    }
    return pid;
}

/* -------- API -------- */

void hal_audio_init(void) { ensure_sigchld_autoreap(); }

void hal_audio_play(const char *path)
{
    hal_audio_play_vol(path, 100);
}

void hal_audio_play_vol(const char *path, int volume_pct)
{
    if (!path) return;
    if (access(path, F_OK) != 0) {
        fprintf(stderr, "[HAL-audio] play: file not found: %s\n", path);
        return;
    }
    fprintf(stderr, "[HAL-audio] play: %s vol=%d\n", path, volume_pct);
    spawn_player(path, volume_pct, 0);
}

void hal_audio_play_sync(const char *path)
{
    hal_audio_play_sync_vol(path, 100);
}

void hal_audio_play_sync_vol(const char *path, int volume_pct)
{
    if (!path) return;
    if (access(path, F_OK) != 0) {
        fprintf(stderr, "[HAL-audio] sync: file not found: %s\n", path);
        return;
    }
    fprintf(stderr, "[HAL-audio] sync play: %s vol=%d\n", path, volume_pct);
    spawn_player(path, volume_pct, 1);
}

void hal_audio_stop(void)   {}
void hal_audio_deinit(void) {}

/* -------- key click -------- */

static char g_click_path[256];
static int  g_click_ready = 0;
#define CLICK_VOL 100

int hal_audio_click_init(const char *wav_path)
{
    if (!wav_path || access(wav_path, F_OK) != 0) {
        fprintf(stderr, "[HAL-click] file not found: %s\n", wav_path ? wav_path : "(null)");
        return -1;
    }
    snprintf(g_click_path, sizeof(g_click_path), "%s", wav_path);
    g_click_ready = 1;
    ensure_sigchld_autoreap();
    fprintf(stderr, "[HAL-click] loaded %s\n", wav_path);
    return 0;
}

void hal_audio_click_play(void)
{
    if (!g_click_ready) return;
    fprintf(stderr, "[HAL-click] play\n");
    spawn_player(g_click_path, CLICK_VOL, 0);
}

void hal_audio_click_deinit(void) { g_click_ready = 0; }
