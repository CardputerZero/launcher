#include "hal/hal_filesystem.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/inotify.h>

int hal_dir_list(const char *path, hal_dirent_t *entries, int max_entries, int *out_count)
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

struct hal_watcher {
    int inotify_fd;
    int watch_fd;
};

hal_watcher_t hal_dir_watch_start(const char *path)
{
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) return NULL;
    int wd = inotify_add_watch(fd, path, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd < 0) { close(fd); return NULL; }
    struct hal_watcher *w = (struct hal_watcher *)malloc(sizeof(struct hal_watcher));
    w->inotify_fd = fd;
    w->watch_fd = wd;
    return w;
}

int hal_dir_watch_poll(hal_watcher_t watcher)
{
    if (!watcher) return -1;
    char buf[1024] __attribute__((aligned(8)));
    ssize_t n = read(watcher->inotify_fd, buf, sizeof(buf));
    return (n > 0) ? 1 : 0;
}

void hal_dir_watch_stop(hal_watcher_t watcher)
{
    if (!watcher) return;
    close(watcher->inotify_fd);
    free(watcher);
}
