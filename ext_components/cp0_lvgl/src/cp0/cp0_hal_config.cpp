#include "hal/hal_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONFIG_DIR  "/var/lib/applaunch"
#define CONFIG_FILE CONFIG_DIR "/settings"
#define MAX_ENTRIES 32
#define KEY_MAX     64
#define VAL_MAX     256

struct config_entry {
    char key[KEY_MAX];
    char val[VAL_MAX];
};

static struct config_entry s_entries[MAX_ENTRIES];
static int s_count = 0;
static int s_loaded = 0;

static void ensure_loaded(void)
{
    if (s_loaded) return;
    s_loaded = 1;
    hal_config_init();
}

void hal_config_init(void)
{
    s_count = 0;
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) return;

    char line[KEY_MAX + VAL_MAX + 4];
    while (fgets(line, sizeof(line), fp) && s_count < MAX_ENTRIES) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = line;
        char *val = eq + 1;
        strncpy(s_entries[s_count].key, key, KEY_MAX - 1);
        strncpy(s_entries[s_count].val, val, VAL_MAX - 1);
        s_count++;
    }
    fclose(fp);
}

static int find_entry(const char *key)
{
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_entries[i].key, key) == 0)
            return i;
    }
    return -1;
}

int hal_config_get_int(const char *key, int default_val)
{
    ensure_loaded();
    int idx = find_entry(key);
    if (idx < 0) return default_val;
    return atoi(s_entries[idx].val);
}

void hal_config_set_int(const char *key, int val)
{
    ensure_loaded();
    int idx = find_entry(key);
    if (idx < 0) {
        if (s_count >= MAX_ENTRIES) return;
        idx = s_count++;
        strncpy(s_entries[idx].key, key, KEY_MAX - 1);
    }
    snprintf(s_entries[idx].val, VAL_MAX, "%d", val);
}

const char *hal_config_get_str(const char *key, const char *default_val)
{
    ensure_loaded();
    int idx = find_entry(key);
    if (idx < 0) return default_val;
    return s_entries[idx].val;
}

void hal_config_set_str(const char *key, const char *val)
{
    ensure_loaded();
    int idx = find_entry(key);
    if (idx < 0) {
        if (s_count >= MAX_ENTRIES) return;
        idx = s_count++;
        strncpy(s_entries[idx].key, key, KEY_MAX - 1);
    }
    strncpy(s_entries[idx].val, val, VAL_MAX - 1);
}

void hal_config_save(void)
{
    mkdir(CONFIG_DIR, 0755);
    FILE *fp = fopen(CONFIG_FILE, "w");
    if (!fp) return;
    for (int i = 0; i < s_count; i++)
        fprintf(fp, "%s=%s\n", s_entries[i].key, s_entries[i].val);
    fclose(fp);
    sync();
}
