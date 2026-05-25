#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "compat/input_keys.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "thpool.h"

extern threadpool g_launch_thread_pool;

typedef void (*switch_cb_t)(lv_event_t *);


#define ROTATE_LEFT(arr, start, end)                   \
    do                                                 \
    {                                                  \
        typeof((arr)[0]) _tmp = (arr)[(start)];        \
        memmove(&(arr)[(start)], &(arr)[(start) + 1],  \
                ((end) - (start)) * sizeof((arr)[0])); \
        (arr)[(end)] = _tmp;                           \
    } while (0)

#define ROTATE_RIGHT(arr, start, end)                  \
    do                                                 \
    {                                                  \
        typeof((arr)[0]) _tmp = (arr)[(end)];          \
        memmove(&(arr)[(start) + 1], &(arr)[(start)],  \
                ((end) - (start)) * sizeof((arr)[0])); \
        (arr)[(start)] = _tmp;                         \
    } while (0)

lv_obj_t *launch_circle[100];

// ==================== 5个槽位的标准坐标 ====================

static const lv_coord_t SLOT_X[] = {-177, -99, 0, 99, 177, -177, -99, 0, 99, 177};
static const lv_coord_t SLOT_Y[] = {4, -6, -16, -6, 4, LABEL_Y_SIDE, LABEL_Y_SIDE, LABEL_Y_CENTER, LABEL_Y_SIDE, LABEL_Y_SIDE};
static const lv_coord_t SLOT_W[] = {61, 80, 100, 80, 61};
static const lv_coord_t SLOT_H[] = {61, 80, 100, 80, 61};

static bool is_animating = false;
static switch_cb_t pending_switch = NULL;

static int Panel_current_pos = 2;
static int switch_current_pos = 11;


// ============================================================
// audio
// ============================================================

static ma_context g_audio_context;
static ma_engine  g_audio_engine;
static ma_device_id g_audio_device_id;
static ma_sound g_sound_switch;
static ma_sound g_sound_enter;

static int g_audio_inited = 0;
static int g_audio_sounds_loaded = 0;

static pthread_mutex_t g_audio_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * 复制字符串，避免 caudio_path() 如果返回静态 buffer 时被覆盖。
 */
static char *audio_strdup_safe(const char *s)
{
    if (s == NULL) {
        return NULL;
    }

    size_t len = strlen(s);
    char *p = (char *)malloc(len + 1);
    if (p == NULL) {
        return NULL;
    }

    memcpy(p, s, len + 1);
    return p;
}

/**
 * 查找 ES8388 / ES8389 播放设备。
 *
 * 注意：
 * 这里查找的是 PulseAudio / PipeWire-Pulse 暴露出来的播放设备名称。
 * 如果 PulseAudio 中设备名称不包含 ES8388 / ES8389，
 * 可以改成直接使用默认设备。
 */
static ma_result audio_find_es8388_device(ma_context *context, ma_device_id *outDeviceID)
{
    ma_device_info *pPlaybackInfos = NULL;
    ma_uint32 playbackCount = 0;

    ma_device_info *pCaptureInfos = NULL;
    ma_uint32 captureCount = 0;

    ma_result result = ma_context_get_devices(
        context,
        &pPlaybackInfos,
        &playbackCount,
        &pCaptureInfos,
        &captureCount
    );

    if (result != MA_SUCCESS) {
        fprintf(stderr, "[AUDIO] ma_context_get_devices failed: %d\n", result);
        return result;
    }

    printf("[AUDIO] PulseAudio playback devices:\n");

    for (ma_uint32 i = 0; i < playbackCount; i++) {
        printf("[AUDIO] [%u] %s%s\n",
               i,
               pPlaybackInfos[i].name,
               pPlaybackInfos[i].isDefault ? " [default]" : "");

        if (strstr(pPlaybackInfos[i].name, "ES8388") != NULL ||
            strstr(pPlaybackInfos[i].name, "ES8389") != NULL) {

            *outDeviceID = pPlaybackInfos[i].id;

            printf("[AUDIO] selected PulseAudio playback device: %s\n",
                   pPlaybackInfos[i].name);

            return MA_SUCCESS;
        }
    }

    fprintf(stderr, "[AUDIO] ES8388/ES8389 PulseAudio playback device not found\n");
    return MA_DOES_NOT_EXIST;
}

/**
 * 初始化音频系统。
 *
 * 这里只使用 PulseAudio 后端。
 *
 * 也就是说 miniaudio 会通过：
 *
 *     ma_backend_pulseaudio
 *
 * 连接到 PulseAudio 或 pipewire-pulse。
 *
 * 不会直接打开 ALSA 设备。
 */
int audio_system_init(void)
{
    pthread_mutex_lock(&g_audio_mutex);

    if (g_audio_inited) {
        pthread_mutex_unlock(&g_audio_mutex);
        return 0;
    }

    ma_result result;

    /*
     * 只启用 PulseAudio 后端。
     *
     * 不要使用：
     *
     *     ma_backend_alsa
     *
     * 如果系统实际使用 PipeWire，只要 pipewire-pulse 正常运行，
     * PulseAudio 后端也可以正常工作。
     */
    ma_backend backends[] = {
        ma_backend_pulseaudio
    };

    result = ma_context_init(
        backends,
        sizeof(backends) / sizeof(backends[0]),
        NULL,
        &g_audio_context
    );

    if (result != MA_SUCCESS) {
        fprintf(stderr, "[AUDIO] ma_context_init PulseAudio failed: %d\n", result);
        pthread_mutex_unlock(&g_audio_mutex);
        return -1;
    }

    /*
     * 查找 ES8388 / ES8389 播放设备。
     *
     * 如果你想直接使用 PulseAudio 默认输出设备，
     * 可以删除 audio_find_es8388_device() 这一步，
     * 并且把 engineConfig.pPlaybackDeviceID 设置为 NULL。
     */
    // result = audio_find_es8388_device(&g_audio_context, &g_audio_device_id);

    // if (result != MA_SUCCESS) {
    //     fprintf(stderr, "[AUDIO] audio_find_es8388_device failed: %d\n", result);
    //     ma_context_uninit(&g_audio_context);
    //     pthread_mutex_unlock(&g_audio_mutex);
    //     return -1;
    // }

    ma_engine_config engineConfig = ma_engine_config_init();

    /*
     * 使用 PulseAudio context。
     */
    engineConfig.pContext = &g_audio_context;

    /*
     * 指定 PulseAudio 播放设备。
     *
     * 如果想使用默认 PulseAudio 设备，可以改成：
     *
     *     engineConfig.pPlaybackDeviceID = NULL;
     */
    // engineConfig.pPlaybackDeviceID = &g_audio_device_id;
    engineConfig.pPlaybackDeviceID = NULL;

    /*
     * 固定输出格式。
     *
     * 如果你的 PulseAudio / PipeWire 默认采样率不是 48000，
     * 也可以改成 44100。
     */
    engineConfig.channels = 2;
    engineConfig.sampleRate = 48000;

    result = ma_engine_init(&engineConfig, &g_audio_engine);

    if (result != MA_SUCCESS) {
        fprintf(stderr, "[AUDIO] ma_engine_init PulseAudio failed: %d\n", result);
        ma_context_uninit(&g_audio_context);
        pthread_mutex_unlock(&g_audio_mutex);
        return -1;
    }

    g_audio_inited = 1;

    printf("[AUDIO] audio system initialized with PulseAudio backend\n");

    pthread_mutex_unlock(&g_audio_mutex);
    return 0;
}

/**
 * 预加载 switch.wav 和 enter.wav。
 *
 * 使用 MA_SOUND_FLAG_DECODE，表示初始化时解码到内存。
 * 播放时不会再重复打开 wav 文件。
 */
int audio_load_sounds(void)
{
    if (!g_audio_inited) {
        if (audio_system_init() != 0) {
            return -1;
        }
    }

    pthread_mutex_lock(&g_audio_mutex);

    if (g_audio_sounds_loaded) {
        pthread_mutex_unlock(&g_audio_mutex);
        return 0;
    }

    ma_result result;

    char *switch_path = audio_strdup_safe(caudio_path("switch.wav"));
    if (switch_path == NULL) {
        fprintf(stderr, "[AUDIO] malloc switch_path failed\n");
        pthread_mutex_unlock(&g_audio_mutex);
        return -1;
    }

    char *enter_path = audio_strdup_safe(caudio_path("enter.wav"));
    if (enter_path == NULL) {
        fprintf(stderr, "[AUDIO] malloc enter_path failed\n");
        free(switch_path);
        pthread_mutex_unlock(&g_audio_mutex);
        return -1;
    }

    result = ma_sound_init_from_file(
        &g_audio_engine,
        switch_path,
        MA_SOUND_FLAG_DECODE,
        NULL,
        NULL,
        &g_sound_switch
    );

    if (result != MA_SUCCESS) {
        fprintf(stderr,
                "[AUDIO] load switch.wav failed: %d, path=%s\n",
                result,
                switch_path);

        free(switch_path);
        free(enter_path);
        pthread_mutex_unlock(&g_audio_mutex);
        return -1;
    }

    result = ma_sound_init_from_file(
        &g_audio_engine,
        enter_path,
        MA_SOUND_FLAG_DECODE,
        NULL,
        NULL,
        &g_sound_enter
    );

    if (result != MA_SUCCESS) {
        fprintf(stderr,
                "[AUDIO] load enter.wav failed: %d, path=%s\n",
                result,
                enter_path);

        ma_sound_uninit(&g_sound_switch);

        free(switch_path);
        free(enter_path);
        pthread_mutex_unlock(&g_audio_mutex);
        return -1;
    }

    free(switch_path);
    free(enter_path);

    g_audio_sounds_loaded = 1;

    printf("[AUDIO] sounds loaded\n");

    pthread_mutex_unlock(&g_audio_mutex);
    return 0;
}

/**
 * 播放已经加载的音效。
 *
 * 如果上一次还没播完，会停止并从头播放。
 */
static void audio_play_loaded_sound(ma_sound *sound)
{
    if (!g_audio_sounds_loaded) {
        if (audio_load_sounds() != 0) {
            return;
        }
    }

    pthread_mutex_lock(&g_audio_mutex);

    ma_sound_stop(sound);
    ma_sound_seek_to_pcm_frame(sound, 0);
    ma_sound_start(sound);

    pthread_mutex_unlock(&g_audio_mutex);
}

/**
 * 播放切换音效。
 */
void audio_play_switch(void)
{
    audio_play_loaded_sound(&g_sound_switch);
}

/**
 * 播放确认音效。
 */
void audio_play_enter(void)
{
    audio_play_loaded_sound(&g_sound_enter);
}

/**
 * 兼容旧的 play_audio(path) 接口。
 *
 * 注意：
 * 这个接口不会预加载文件。
 * 但是不会重复打开 PulseAudio 设备。
 */
void play_audio(char *path)
{
    if (path == NULL) {
        return;
    }

    if (!g_audio_inited) {
        if (audio_system_init() != 0) {
            return;
        }
    }

    pthread_mutex_lock(&g_audio_mutex);

    ma_result result = ma_engine_play_sound(&g_audio_engine, path, NULL);

    if (result != MA_SUCCESS) {
        fprintf(stderr, "[AUDIO] play_audio failed: %d, path=%s\n", result, path);
    }

    pthread_mutex_unlock(&g_audio_mutex);
}

/**
 * 释放音频系统。
 *
 * 如果你的程序有退出流程，退出时调用这个函数。
 */
void audio_system_uninit(void)
{
    pthread_mutex_lock(&g_audio_mutex);

    if (g_audio_sounds_loaded) {
        ma_sound_uninit(&g_sound_switch);
        ma_sound_uninit(&g_sound_enter);
        g_audio_sounds_loaded = 0;
    }

    if (g_audio_inited) {
        ma_engine_uninit(&g_audio_engine);
        ma_context_uninit(&g_audio_context);
        g_audio_inited = 0;
    }

    pthread_mutex_unlock(&g_audio_mutex);
}

// ============================================================
// 初始化
// ============================================================

void launch_circle_init()
{
    launch_circle[0] = ui_outPanelzuo;
    launch_circle[1] = ui_zuoPanel;
    launch_circle[2] = ui_switchPanel;
    launch_circle[3] = ui_youPanel;
    launch_circle[4] = ui_outPanelyou;

    launch_circle[5] = ui_zuoLabelout;
    launch_circle[6] = ui_zuoLabel;
    launch_circle[7] = ui_switchLabel;
    launch_circle[8] = ui_youLabel;
    launch_circle[9] = ui_youLabelout;

    launch_circle[10] = ui_Panel4;
    launch_circle[11] = ui_Panel3;
    launch_circle[12] = ui_Panel5;
    launch_circle[13] = ui_Panel6;
    launch_circle[14] = ui_Panel7;
    launch_circle[15] = ui_Panel8;
    launch_circle[16] = ui_Panel9;
    launch_circle[17] = ui_Panel10;


}


// ============================================================
// switch panel style
// ============================================================

static void switchpanleEnable(int obj_index, int enable)
{
    lv_obj_t *obj = launch_circle[obj_index];

    if (enable)
    {
        lv_obj_set_width(obj, 10);
        lv_obj_set_height(obj, 10);
        lv_obj_set_align(obj, LV_ALIGN_CENTER);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_color(obj, lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj, lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else
    {
        lv_obj_set_width(obj, 5);
        lv_obj_set_height(obj, 5);
        lv_obj_set_align(obj, LV_ALIGN_CENTER);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_color(obj, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}


static void switchpanleEnableClick(int obj_index, int enable)
{
    lv_obj_t *obj = launch_circle[obj_index];

    if (enable)
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
    else
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
}


// ============================================================
// 将面板强制设定到指定槽位
// ============================================================

static void snap_panel_to_slot(lv_obj_t *panel, int slot)
{
    lv_obj_set_x(panel, SLOT_X[slot]);
    lv_obj_set_y(panel, SLOT_Y[slot]);
    lv_obj_set_width(panel, SLOT_W[slot]);
    lv_obj_set_height(panel, SLOT_H[slot]);

    if (slot == 0 || slot == 4)
    {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }
}


// ============================================================
// 将标签强制设定到指定槽位
// ============================================================

static void snap_label_to_slot(lv_obj_t *label, int slot)
{
    lv_obj_set_x(label, SLOT_X[slot]);
    lv_obj_set_y(label, SLOT_Y[slot]);

    if (slot == 5 || slot == 9)
    {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}


// ============================================================
// 动画结束后校正所有面板位置
// ============================================================

static void snap_all_panels(lv_anim_t *a)
{
    for (int i = 0; i < 5; i++)
    {
        snap_panel_to_slot(launch_circle[i], i);
    }

    for (int i = 5; i < 10; i++)
    {
        snap_label_to_slot(launch_circle[i], i);
    }

    is_animating = false;

    // Reset border colors: center=bright, sides=dark
    for (int i = 0; i < 5; i++) {
        uint32_t color = (i == 2) ? BORDER_COLOR_CENTER : BORDER_COLOR_SIDE;
        lv_obj_set_style_border_color(launch_circle[i], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Reset all label fonts to bold
    for (int i = 5; i < 10; i++) {
        lv_obj_set_style_text_font(launch_circle[i], g_font_bold_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (pending_switch) {
        switch_cb_t cb = pending_switch;
        pending_switch = NULL;
        cb(NULL);
    }
}


// ============================================================
// 向右切换，点右箭头时调用
// ============================================================

void switchyou(lv_event_t *e)
{
    if (is_animating)
    {
        pending_switch = &switchyou;
        return;
    }

    is_animating = true;

    lv_obj_clear_flag(launch_circle[0], LV_OBJ_FLAG_HIDDEN);

    zuopanelout2you_Animation(launch_circle[0], 0, NULL);
    zuopanel2you_Animation(launch_circle[1], 0, NULL);
    switchpanel2you_Animation(launch_circle[2], 0, NULL);
    youpanel2you_Animation(launch_circle[3], 0, snap_all_panels);

    snap_panel_to_slot(launch_circle[4], 0);

    lv_obj_clear_flag(launch_circle[5], LV_OBJ_FLAG_HIDDEN);

    zuolabelout2you_Animation(launch_circle[5], 0, NULL);
    zuolabel2you_Animation(launch_circle[6], 0, NULL);
    switchlabel2you_Animation(launch_circle[7], 0, NULL);
    youlabel2you_Animation(launch_circle[8], 0, NULL);

    snap_label_to_slot(launch_circle[9], 5);

    cpp_app_you(launch_circle[4], launch_circle[9]);

    switchpanleEnableClick(2, 0);
    ROTATE_RIGHT(launch_circle, 0, 4);
    switchpanleEnableClick(2, 1);

    ROTATE_RIGHT(launch_circle, 5, 9);

    switchpanleEnable(switch_current_pos, 0);

    switch_current_pos = switch_current_pos == 10 ? 17 : switch_current_pos - 1;

    switchpanleEnable(switch_current_pos, 1);
}


// ============================================================
// 向左切换，点左箭头时调用
// ============================================================

void switchzuo(lv_event_t *e)
{
    if (is_animating)
    {
        pending_switch = &switchzuo;
        return;
    }

    is_animating = true;

    lv_obj_clear_flag(launch_circle[4], LV_OBJ_FLAG_HIDDEN);

    zuopanelout2zuo_Animation(launch_circle[4], 0, NULL);
    youpanel2zuo_Animation(launch_circle[3], 0, NULL);
    switchpanel2zuo_Animation(launch_circle[2], 0, NULL);
    zuopanel2zuo_Animation(launch_circle[1], 0, snap_all_panels);

    snap_panel_to_slot(launch_circle[0], 4);

    lv_obj_clear_flag(launch_circle[9], LV_OBJ_FLAG_HIDDEN);

    zuolabelout2zuo_Animation(launch_circle[9], 0, NULL);
    youlabel2zuo_Animation(launch_circle[8], 0, NULL);
    switchlabel2zuo_Animation(launch_circle[7], 0, NULL);
    zuolabel2zuo_Animation(launch_circle[6], 0, NULL);

    snap_label_to_slot(launch_circle[5], 9);

    cpp_app_zuo(launch_circle[0], launch_circle[5]);

    switchpanleEnableClick(2, 0);
    ROTATE_LEFT(launch_circle, 0, 4);
    switchpanleEnableClick(2, 1);

    ROTATE_LEFT(launch_circle, 5, 9);

    switchpanleEnable(switch_current_pos, 0);

    switch_current_pos = switch_current_pos == 17 ? 10 : switch_current_pos + 1;

    switchpanleEnable(switch_current_pos, 1);
}


// ============================================================
// screen / app
// ============================================================

void go_back_home(lv_event_t *e)
{
    lv_disp_load_scr(ui_Screen1);
    lv_indev_set_group(lv_indev_get_next(NULL), Screen1group);
}


void ui_event_Screen1(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_KEYBOARD)
    {
        main_key_switch(e);
    }
}


void app_launch(lv_event_t *e)
{
    cpp_app_launch();
}


static uint32_t fzxc_to_arrow(uint32_t key)
{
    switch (key)
    {
    case KEY_F:
        return KEY_UP;

    case KEY_X:
        return KEY_DOWN;

    case KEY_Z:
        return KEY_LEFT;

    case KEY_C:
        return KEY_RIGHT;

    default:
        return key;
    }
}


// ============================================================
// key handler
// ============================================================

int lvping_lock = 0;

void main_key_switch(lv_event_t *e)
{
    struct key_item *elm = (struct key_item *)lv_event_get_param(e);
    uint32_t code = fzxc_to_arrow(elm->key_code);

    printf("[LAUNCHER] main_key_switch raw=%u->code=%u state=%s sym=%s\n",
           elm->key_code,
           code,
           kbd_state_name(elm->key_state),
           elm->sym_name);

    if (elm->key_state)
    {
        switch (code)
        {
        case KEY_UP:
            break;

        case KEY_DOWN:
            break;

        case KEY_LEFT:
        {
            /*
             * 原来这里是：
             * thpool_add_work(g_launch_thread_pool, play_audio, caudio_path("switch.wav"));
             *
             * 现在改成直接播放预加载音效。
             */
            if (!lvping_lock)
            {
                audio_play_switch();
                switchyou(NULL);
            }
        }
        break;

        case KEY_RIGHT:
        {
            if (!lvping_lock)
            {
                audio_play_switch();
                switchzuo(NULL);
            }
        }
        break;

        default:
            break;
        }
    }
    else if (code == KEY_ENTER)
    {
        audio_play_enter();
        app_launch(NULL);
    }
    else if (code == KEY_F12)
    {
        static lv_obj_t *green_bg;
        if (lvping_lock == 0)
        {
            lvping_lock = 1;
            green_bg = lv_obj_create(lv_scr_act());
            lv_obj_set_size(green_bg, 320, 170);
            lv_obj_align(green_bg, LV_ALIGN_TOP_LEFT, 0, 0);

            lv_obj_set_style_bg_color(green_bg, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(green_bg, LV_OPA_COVER, LV_PART_MAIN);

            lv_obj_set_style_border_width(green_bg, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(green_bg, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(green_bg, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(green_bg, 0, LV_PART_MAIN);
        }
        else
        {
            lvping_lock = 0;
            lv_obj_del(green_bg);
        }
    }
}