#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[256];
    int  is_dir;
} hal_dirent_t;

int hal_dir_list(const char *path, hal_dirent_t *entries,
                 int max_entries, int *out_count);

typedef struct hal_watcher *hal_watcher_t;

hal_watcher_t hal_dir_watch_start(const char *path);
int           hal_dir_watch_poll(hal_watcher_t watcher);
void          hal_dir_watch_stop(hal_watcher_t watcher);

#ifdef __cplusplus
}
#endif
