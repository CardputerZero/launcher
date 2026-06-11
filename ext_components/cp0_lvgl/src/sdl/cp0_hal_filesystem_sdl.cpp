#include "hal/hal_filesystem.h"
#include <stdio.h>   // snprintf
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>

int hal_dir_list(const char *path, hal_dirent_t *entries, int max_entries, int *out_count)
{
    *out_count = 0;
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (fd.cFileName[0] == '.') continue;
        if (*out_count >= max_entries) break;
        strncpy(entries[*out_count].name, fd.cFileName, 255);
        entries[*out_count].name[255] = '\0';
        entries[*out_count].is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        (*out_count)++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 0;
}

hal_watcher_t hal_dir_watch_start(const char *path) { (void)path; return NULL; }
int           hal_dir_watch_poll(hal_watcher_t w) { (void)w; return 0; }
void          hal_dir_watch_stop(hal_watcher_t w) { (void)w; }

#else
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>

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
    char path[512];
    time_t last_mtime;
};

hal_watcher_t hal_dir_watch_start(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    struct hal_watcher *w = (struct hal_watcher *)malloc(sizeof(struct hal_watcher));
    strncpy(w->path, path, 511);
    w->path[511] = '\0';
    w->last_mtime = st.st_mtime;
    return w;
}

int hal_dir_watch_poll(hal_watcher_t watcher)
{
    if (!watcher) return -1;
    struct stat st;
    if (stat(watcher->path, &st) != 0) return -1;
    if (st.st_mtime != watcher->last_mtime) {
        watcher->last_mtime = st.st_mtime;
        return 1;
    }
    return 0;
}

void hal_dir_watch_stop(hal_watcher_t watcher)
{
    free(watcher);
}

#endif
