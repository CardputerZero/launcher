#include "hal/hal_paths.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char s_data_dir[512]         = ".";
static char s_applications_dir[512] = "./applications";
static char s_store_cache_dir[512]  = "./store_cache";
static char s_lock_file[512]        = "/tmp/M5CardputerZero-APPLaunch_fcntl.lock";
static char s_font_dir[512]         = "./APPLaunch/share/font";
static char s_font_regular[512]     = "./APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf";
static char s_font_mono[512]        = "./APPLaunch/share/font/LiberationMono-Regular.ttf";
static char s_images_dir[512]       = "./APPLaunch/share/images";
static char s_audio_dir[512]        = "./APPLaunch/share/audio";

static char s_store_sync_cmd[512]   = "python store_cache_sync.py";

void hal_paths_init(const char *exe_dir)
{
    if (!exe_dir) exe_dir = ".";
    snprintf(s_data_dir,         sizeof(s_data_dir),         "%s", exe_dir);
    snprintf(s_applications_dir, sizeof(s_applications_dir), "%s/applications", exe_dir);
    snprintf(s_store_cache_dir,  sizeof(s_store_cache_dir),  "%s/store_cache", exe_dir);
    snprintf(s_images_dir,       sizeof(s_images_dir),       "%s/APPLaunch/share/images", exe_dir);
    snprintf(s_font_dir,         sizeof(s_font_dir),         "%s/APPLaunch/share/font", exe_dir);
    snprintf(s_audio_dir,        sizeof(s_audio_dir),        "%s/APPLaunch/share/audio", exe_dir);
    snprintf(s_font_regular,     sizeof(s_font_regular),     "%s/APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf", exe_dir);
    snprintf(s_font_mono,        sizeof(s_font_mono),        "%s/APPLaunch/share/font/LiberationMono-Regular.ttf", exe_dir);
    snprintf(s_store_sync_cmd,   sizeof(s_store_sync_cmd),   "python %s/bin/store_cache_sync.py", exe_dir);
}

const char *hal_path_data_dir(void)         { return s_data_dir; }
const char *hal_path_applications_dir(void) { return s_applications_dir; }
const char *hal_path_store_cache_dir(void)  { return s_store_cache_dir; }
const char *hal_path_lock_file(void)        { return s_lock_file; }
const char *hal_path_font_dir(void)         { return s_font_dir; }
const char *hal_path_font_regular(void)     { return s_font_regular; }
const char *hal_path_font_mono(void)        { return s_font_mono; }
const char *hal_path_keyboard_device(void)  { return NULL; }
const char *hal_path_keyboard_map(void)     { return NULL; }
const char *hal_path_store_sync_cmd(void)   { return s_store_sync_cmd; }
const char *hal_path_images_dir(void)       { return s_images_dir; }
const char *hal_path_audio_dir(void)       { return s_audio_dir; }