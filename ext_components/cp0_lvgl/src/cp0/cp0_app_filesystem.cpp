#include "cp0_lvgl_app.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/inotify.h>

int cp0_dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count)
{
    *out_count = 0;
    DIR *dir = opendir(path);
    if (!dir) return -1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (*out_count >= max_entries) break;
        strncpy(entries[*out_count].name, ent->d_name, 255);
        entries[*out_count].name[255] = '\0';
        entries[*out_count].is_dir = (ent->d_type == DT_DIR) ? 1 : 0;
        (*out_count)++;
    }
    closedir(dir);
    return 0;
}

struct cp0_dir_watcher {
    int inotify_fd;
    int watch_fd;
};

cp0_watcher_t cp0_dir_watch_start(const char *path)
{
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) return NULL;
    int wd = inotify_add_watch(fd, path, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd < 0) { close(fd); return NULL; }
    struct cp0_dir_watcher *w = (struct cp0_dir_watcher *)malloc(sizeof(struct cp0_dir_watcher));
    w->inotify_fd = fd;
    w->watch_fd = wd;
    return w;
}

int cp0_dir_watch_poll(cp0_watcher_t watcher)
{
    if (!watcher) return -1;
    char buf[1024] __attribute__((aligned(8)));
    struct cp0_dir_watcher *w = (struct cp0_dir_watcher *)watcher;
    ssize_t n = read(w->inotify_fd, buf, sizeof(buf));
    return (n > 0) ? 1 : 0;
}

void cp0_dir_watch_stop(cp0_watcher_t watcher)
{
    if (!watcher) return;
    struct cp0_dir_watcher *w = (struct cp0_dir_watcher *)watcher;
    close(w->inotify_fd);
    free(w);
}
