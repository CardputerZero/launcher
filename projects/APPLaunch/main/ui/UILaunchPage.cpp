#include "UILaunchPage.h"

#include "Launch.h"
#include "lvgl/src/widgets/gif/lv_gif.h"
#include "sample_log.h"
#include "compat/input_keys.h"
#include <utility>

#include "Animation/ui_launcher_animation.h"

#include <cstring>

extern "C" {

typedef void (*switch_cb_t)(lv_event_t *);


#define ROTATE_LEFT(arr, start, end)                   \
    do                                                 \
    {                                                  \
        __typeof__((arr)[0]) _tmp = (arr)[(start)];        \
        memmove(&(arr)[(start)], &(arr)[(start) + 1],  \
                ((end) - (start)) * sizeof((arr)[0])); \
        (arr)[(end)] = _tmp;                           \
    } while (0)

#define ROTATE_RIGHT(arr, start, end)                  \
    do                                                 \
    {                                                  \
        __typeof__((arr)[0]) _tmp = (arr)[(end)];          \
        memmove(&(arr)[(start) + 1], &(arr)[(start)],  \
                ((end) - (start)) * sizeof((arr)[0])); \
        (arr)[(start)] = _tmp;                         \
    } while (0)

lv_obj_t *launch_circle[100];

// ==================== standard coordinates for 5 slots ====================

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

static void audio_play_ui_asset(const char *name)
{
    cp0_signal_system_play_asset(name);
}

static void audio_play_switch(void)
{
    audio_play_ui_asset("switch.wav");
}

static void audio_play_enter(void)
{
    audio_play_ui_asset("enter.wav");
}

// ============================================================
// Initialize
// ============================================================

void launch_circle_init()
{
    launch_circle[0] = ui_leftOuterPanel;
    launch_circle[1] = ui_leftPanel;
    launch_circle[2] = ui_switchPanel;
    launch_circle[3] = ui_rightPanel;
    launch_circle[4] = ui_rightOuterPanel;

    launch_circle[5] = ui_leftOuterLabel;
    launch_circle[6] = ui_leftLabel;
    launch_circle[7] = ui_switchLabel;
    launch_circle[8] = ui_rightLabel;
    launch_circle[9] = ui_rightOuterLabel;

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
// Force the panel to the specified slot
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
// Force the label to the specified slot
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
// Correct all panel positions after animation ends
// ============================================================

static void snap_all_panels()
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
// Switch right; called when the right arrow is clicked
// ============================================================

void switch_right(lv_event_t *e)
{
    if (is_animating)
    {
        pending_switch = &switch_right;
        return;
    }

    is_animating = true;

    lv_obj_clear_flag(launch_circle[0], LV_OBJ_FLAG_HIDDEN);

    launcher_home_animation::animate_right(launch_circle, snap_all_panels);

    snap_panel_to_slot(launch_circle[4], 0);

    lv_obj_clear_flag(launch_circle[5], LV_OBJ_FLAG_HIDDEN);

    snap_label_to_slot(launch_circle[9], 5);

    cpp_app_right(launch_circle[4], launch_circle[9]);

    switchpanleEnableClick(2, 0);
    ROTATE_RIGHT(launch_circle, 0, 4);
    switchpanleEnableClick(2, 1);

    ROTATE_RIGHT(launch_circle, 5, 9);

    switchpanleEnable(switch_current_pos, 0);

    switch_current_pos = switch_current_pos == 10 ? 17 : switch_current_pos - 1;

    switchpanleEnable(switch_current_pos, 1);
}


// ============================================================
// Switch left; called when the left arrow is clicked
// ============================================================

void switch_left(lv_event_t *e)
{
    if (is_animating)
    {
        pending_switch = &switch_left;
        return;
    }

    is_animating = true;

    lv_obj_clear_flag(launch_circle[4], LV_OBJ_FLAG_HIDDEN);

    launcher_home_animation::animate_left(launch_circle, snap_all_panels);

    snap_panel_to_slot(launch_circle[0], 4);

    lv_obj_clear_flag(launch_circle[9], LV_OBJ_FLAG_HIDDEN);

    snap_label_to_slot(launch_circle[5], 9);

    cpp_app_left(launch_circle[0], launch_circle[5]);

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
    UILaunchPage::bind_home_input_group();
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

static int lvping_lock = 0;

void main_key_switch(lv_event_t *e)
{
    struct key_item *elm = (struct key_item *)lv_event_get_param(e);
    uint32_t code = fzxc_to_arrow(elm->key_code);

    SLOGI("[LAUNCHER] main_key_switch raw=%u->code=%u state=%s sym=%s",
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
            /* Play the preloaded sound effect directly before switching pages. */
            if (!lvping_lock)
            {
                audio_play_switch();
                switch_right(NULL);
            }
        }
        break;

        case KEY_RIGHT:
        {
            if (!lvping_lock)
            {
                audio_play_switch();
                switch_left(NULL);
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


} // extern "C"

namespace {

char img_path_buf[16][256];
char regular_font_path[512];
char mono_font_path_buf[512];
char gif_path[256];
const char *font_path = nullptr;
const char *mono_font_path = nullptr;
lv_group_t *home_input_group = nullptr;

} // namespace

lv_obj_t *startup_gif = nullptr;

void UILaunchPage::init_images()
{
    struct ImagePath {
        const char **ptr;
        const char *name;
    };

    ImagePath table[] = {
        {&ui_img_zero_png, "zero.png"},
        {&ui_img_time_png, "time_bg.png"},
        {&ui_img_battery_bg_png, "battery_bg.png"},
        {&ui_img_left_png, "left.png"},
        {&ui_img_right_png, "right.png"},
        {&ui_img_zero_logo_w_png, "zero_logo_w.png"},
        {&ui_img_left_logo_png, "left_logo.png"},
        {&ui_img_right_logo_png, "right_logo.png"},
        {&ui_img_detail_info_png, "detail_info.png"},
        {&ui_img_down_logo_png, "down_logo.png"},
        {&ui_img_up_logo_png, "up_logo.png"},
        {&ui_img_camera_png, "camera.png"},
    };

    int count = sizeof(table) / sizeof(table[0]);
    for (int i = 0; i < count && i < 16; ++i) {
        snprintf(img_path_buf[i], sizeof(img_path_buf[i]), "%s", cp0_file_path(table[i].name).c_str());
        *table[i].ptr = img_path_buf[i];
    }
}

void UILaunchPage::init_fonts()
{
    snprintf(regular_font_path, sizeof(regular_font_path), "%s", cp0_file_path("AlibabaPuHuiTi-3-55-Regular.ttf").c_str());
    snprintf(mono_font_path_buf, sizeof(mono_font_path_buf), "%s", cp0_file_path("LiberationMono-Regular.ttf").c_str());
    font_path = regular_font_path;
    mono_font_path = mono_font_path_buf;

    g_font_cn_20 = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 20,
                                           LV_FREETYPE_FONT_STYLE_NORMAL);
    g_font_cn_14 = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 14,
                                           LV_FREETYPE_FONT_STYLE_NORMAL);
    g_font_cn_12 = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 12,
                                           LV_FREETYPE_FONT_STYLE_BOLD);
    g_font_mono_12 = lv_freetype_font_create(mono_font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 12,
                                             LV_FREETYPE_FONT_STYLE_NORMAL);

    char bold_path[512];
    snprintf(bold_path, sizeof(bold_path), "%s", cp0_file_path("Montserrat-Bold.ttf").c_str());
    g_font_bold_20 = lv_freetype_font_create(bold_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 18,
                                             LV_FREETYPE_FONT_STYLE_BOLD);
    g_font_bold_14 = lv_freetype_font_create(bold_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 16,
                                             LV_FREETYPE_FONT_STYLE_BOLD);
    g_font_bold_12 = lv_freetype_font_create(bold_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 12,
                                             LV_FREETYPE_FONT_STYLE_BOLD);

    if (!g_font_cn_20) g_font_cn_20 = (lv_font_t *)&lv_font_montserrat_20;
    if (!g_font_cn_14) g_font_cn_14 = (lv_font_t *)&lv_font_montserrat_14;
    if (!g_font_cn_12) g_font_cn_12 = (lv_font_t *)&lv_font_montserrat_12;
    if (!g_font_mono_12) g_font_mono_12 = (lv_font_t *)&lv_font_montserrat_12;
    if (!g_font_bold_20) g_font_bold_20 = (lv_font_t *)&lv_font_montserrat_18;
    if (!g_font_bold_14) g_font_bold_14 = (lv_font_t *)&lv_font_montserrat_14;
    if (!g_font_bold_12) g_font_bold_12 = (lv_font_t *)&lv_font_montserrat_12;
}

lv_group_t *UILaunchPage::home_input_group()
{
    return ::home_input_group;
}

void UILaunchPage::bind_home_input_group()
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev) {
        lv_indev_set_group(indev, ::home_input_group);
    }
}

void UILaunchPage::init_input_group()
{
    if (::home_input_group) {
        bind_home_input_group();
        return;
    }

    ::home_input_group = lv_group_create();
    lv_group_add_obj(::home_input_group, ui_Screen1);
    bind_home_input_group();
}

void UILaunchPage::load_home_screen()
{
    SLOGI("[HOME] home_screen_load() - loading launcher home screen");
    ui____initial_actions0 = lv_obj_create(NULL);
    lv_disp_load_scr(ui_Screen1);
    UILaunchPage::bind_home_input_group();

    cp0_signal_audio_api_play_asset("startup.mp3");
}

static void ui_event_logo_over(lv_event_t *e)
{
    static int done = 0;
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_READY && !done) {
        done = 1;
        SLOGI("[GIF] first LV_EVENT_READY -> pause + home_screen_load()");
        if (startup_gif) lv_gif_pause(startup_gif);

        UILaunchPage::load_home_screen();
    }
}

void UILaunchPage::start_startup_gif()
{
    snprintf(gif_path, sizeof(gif_path), "%s", cp0_file_path("logo_output.gif").c_str());
    startup_gif = lv_gif_create(NULL);
    lv_gif_set_src(startup_gif, gif_path);
    lv_obj_center(startup_gif);
    lv_obj_add_event_cb(startup_gif, ui_event_logo_over, LV_EVENT_ALL, NULL);
    lv_disp_load_scr(startup_gif);
}

void UILaunchPage::init_ui()
{
    init_images();
    init_fonts();

    LV_EVENT_GET_COMP_CHILD = lv_event_register_id();

    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    create_screen();

    ui_info_bind();
    launch_circle_init();

    init_input_group();

#ifndef APPLAUNCH_STARTUP_ANIMATION
    load_home_screen();
#else
#ifdef HAL_PLATFORM_SDL
    load_home_screen();
#else
    char gif_check[256];
    snprintf(gif_check, sizeof(gif_check), "%s", cp0_file_path("logo_output.gif").c_str());
    FILE *gif_file = fopen(gif_check, "r");
    if (gif_file) {
        fclose(gif_file);
        start_startup_gif();
    } else {
        load_home_screen();
    }
#endif
#endif
}

extern "C" void home_screen_load()
{
    UILaunchPage::load_home_screen();
}

extern "C" void start_startup_gif()
{
    UILaunchPage::start_startup_gif();
}

extern "C" void ui_inita(void)
{
    UILaunchPage::init_ui();
}


UILaunchPage::UILaunchPage(std::shared_ptr<Launch> launch)
    : home_base(), launch_(std::move(launch))
{
}

UILaunchPage::~UILaunchPage() = default;

void UILaunchPage::create_screen()
{
    if (ui_Screen1)
        return;

    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_top(ui_Screen1);
    create_app_container(ui_Screen1);

}

void UILaunchPage::create_top(lv_obj_t *parent)
{
#ifdef APPLAUNCH_LOGO_USE_PNG
    ui_Image1 = lv_img_create(parent);
    lv_img_set_src(ui_Image1, ui_img_zero_png);
    lv_obj_set_width(ui_Image1, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Image1, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Image1, 5);
    lv_obj_set_y(ui_Image1, 5);
    lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_SCROLLABLE);
#else
    ui_Image1 = lv_label_create(parent);
    lv_label_set_text(ui_Image1, "ZERO");
    lv_obj_set_x(ui_Image1, 5);
    lv_obj_set_y(ui_Image1, 2);
    lv_obj_set_style_text_font(ui_Image1, g_font_bold_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Image1, lv_color_hex(0xCCAA00), LV_PART_MAIN | LV_STATE_DEFAULT);
#endif

    // --- WiFi signal strength bars (4 bars, hidden when disconnected) ---
    ui_wifiPanel = lv_obj_create(parent);
    lv_obj_set_width(ui_wifiPanel, 24);
    lv_obj_set_height(ui_wifiPanel, 15);
    lv_obj_set_x(ui_wifiPanel, 210);
    lv_obj_set_y(ui_wifiPanel, 4);
    lv_obj_clear_flag(ui_wifiPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_wifiPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_wifiPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_wifiPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_wifiPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_wifiPanel, LV_OBJ_FLAG_HIDDEN);

    ui_wifiBar1 = lv_obj_create(ui_wifiPanel);
    lv_obj_set_width(ui_wifiBar1, 4);
    lv_obj_set_height(ui_wifiBar1, 6);
    lv_obj_set_align(ui_wifiBar1, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_x(ui_wifiBar1, 0);
    lv_obj_clear_flag(ui_wifiBar1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_wifiBar1, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_wifiBar1, lv_color_hex(0x4D4D4D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_wifiBar1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_wifiBar1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_wifiBar2 = lv_obj_create(ui_wifiPanel);
    lv_obj_set_width(ui_wifiBar2, 4);
    lv_obj_set_height(ui_wifiBar2, 9);
    lv_obj_set_align(ui_wifiBar2, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_x(ui_wifiBar2, 6);
    lv_obj_clear_flag(ui_wifiBar2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_wifiBar2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_wifiBar2, lv_color_hex(0x4D4D4D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_wifiBar2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_wifiBar2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_wifiBar3 = lv_obj_create(ui_wifiPanel);
    lv_obj_set_width(ui_wifiBar3, 4);
    lv_obj_set_height(ui_wifiBar3, 12);
    lv_obj_set_align(ui_wifiBar3, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_x(ui_wifiBar3, 12);
    lv_obj_clear_flag(ui_wifiBar3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_wifiBar3, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_wifiBar3, lv_color_hex(0x4D4D4D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_wifiBar3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_wifiBar3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_wifiBar4 = lv_obj_create(ui_wifiPanel);
    lv_obj_set_width(ui_wifiBar4, 4);
    lv_obj_set_height(ui_wifiBar4, 15);
    lv_obj_set_align(ui_wifiBar4, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_x(ui_wifiBar4, 18);
    lv_obj_clear_flag(ui_wifiBar4, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_wifiBar4, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_wifiBar4, lv_color_hex(0x4D4D4D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_wifiBar4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_wifiBar4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // --- Time status icon ---
    ui_Panel1 = lv_obj_create(parent);
    lv_obj_set_width(ui_Panel1, 40);
    lv_obj_set_height(ui_Panel1, 16);
    lv_obj_set_x(ui_Panel1, 236);
    lv_obj_set_y(ui_Panel1, 4);
    lv_obj_clear_flag(ui_Panel1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_Panel1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Panel1, ui_img_time_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Panel1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_timeLabel = lv_label_create(ui_Panel1);
    lv_obj_set_width(ui_timeLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_timeLabel, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_timeLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_timeLabel, "15:21");
    lv_obj_set_style_text_color(ui_timeLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_timeLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // --- Battery status icon ---
    ui_batteryPanel = lv_obj_create(parent);
    lv_obj_set_width(ui_batteryPanel, 36);
    lv_obj_set_height(ui_batteryPanel, 16);
    lv_obj_set_x(ui_batteryPanel, 280);
    lv_obj_set_y(ui_batteryPanel, 4);
    lv_obj_clear_flag(ui_batteryPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_batteryPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_batteryPanel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_batteryPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_batteryPanel, ui_img_battery_bg_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_batteryPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_batteryPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Bar1 = lv_bar_create(ui_batteryPanel);
    lv_bar_set_value(ui_Bar1, 96, LV_ANIM_OFF);
    lv_bar_set_start_value(ui_Bar1, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_Bar1, 33);
    lv_obj_set_height(ui_Bar1, 14);
    lv_obj_set_align(ui_Bar1, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_Bar1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Bar1, lv_color_hex(0x484847), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_Bar1, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Bar1, lv_color_hex(0x666633), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar1, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_powerLabel = lv_label_create(ui_batteryPanel);
    lv_obj_set_width(ui_powerLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_powerLabel, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_powerLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_powerLabel, "96%");
    lv_obj_set_style_text_color(ui_powerLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_powerLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

}

void UILaunchPage::create_app_container(lv_obj_t *parent)
{
    ::ui_APP_Container = lv_obj_create(parent);
    lv_obj_remove_style_all(::ui_APP_Container);
    lv_obj_set_width(::ui_APP_Container, 320);
    lv_obj_set_height(::ui_APP_Container, 150);
    lv_obj_set_x(::ui_APP_Container, 0);
    lv_obj_set_y(::ui_APP_Container, 10);
    lv_obj_set_align(::ui_APP_Container, LV_ALIGN_CENTER);
    lv_obj_clear_flag(::ui_APP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    ui_Panel4 = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_Panel4, 5);
    lv_obj_set_height(ui_Panel4, 5);
    lv_obj_set_x(ui_Panel4, -35);
    lv_obj_set_y(ui_Panel4, 70);
    lv_obj_set_align(ui_Panel4, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel4, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Panel4, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Panel4, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel4, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel3 = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_Panel3, 8);
    lv_obj_set_height(ui_Panel3, 8);
    lv_obj_set_x(ui_Panel3, -25);
    lv_obj_set_y(ui_Panel3, 70);
    lv_obj_set_align(ui_Panel3, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel3, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Panel3, lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Panel3, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel3, lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel5 = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_Panel5, 5);
    lv_obj_set_height(ui_Panel5, 5);
    lv_obj_set_x(ui_Panel5, -15);
    lv_obj_set_y(ui_Panel5, 70);
    lv_obj_set_align(ui_Panel5, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel5, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Panel5, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Panel5, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel5, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel6 = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_Panel6, 5);
    lv_obj_set_height(ui_Panel6, 5);
    lv_obj_set_x(ui_Panel6, -5);
    lv_obj_set_y(ui_Panel6, 70);
    lv_obj_set_align(ui_Panel6, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel6, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Panel6, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Panel6, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel6, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel7 = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_Panel7, 5);
    lv_obj_set_height(ui_Panel7, 5);
    lv_obj_set_x(ui_Panel7, 5);
    lv_obj_set_y(ui_Panel7, 70);
    lv_obj_set_align(ui_Panel7, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel7, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Panel7, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Panel7, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel7, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel8 = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_Panel8, 5);
    lv_obj_set_height(ui_Panel8, 5);
    lv_obj_set_x(ui_Panel8, 15);
    lv_obj_set_y(ui_Panel8, 70);
    lv_obj_set_align(ui_Panel8, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel8, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Panel8, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel8, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Panel8, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel8, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel8, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel9 = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_Panel9, 5);
    lv_obj_set_height(ui_Panel9, 5);
    lv_obj_set_x(ui_Panel9, 25);
    lv_obj_set_y(ui_Panel9, 70);
    lv_obj_set_align(ui_Panel9, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel9, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Panel9, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel9, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Panel9, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel9, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel9, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel10 = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_Panel10, 5);
    lv_obj_set_height(ui_Panel10, 5);
    lv_obj_set_x(ui_Panel10, 35);
    lv_obj_set_y(ui_Panel10, 70);
    lv_obj_set_align(ui_Panel10, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel10, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Panel10, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel10, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Panel10, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel10, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel10, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_switchLabel = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(ui_switchLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_switchLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_switchLabel, 0);
    lv_obj_set_y(ui_switchLabel, LABEL_Y_CENTER);
    lv_obj_set_align(ui_switchLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_switchLabel, "CLI");
    lv_obj_set_style_text_font(ui_switchLabel, g_font_bold_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_switchLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_switchLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_rightLabel = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(ui_rightLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_rightLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_rightLabel, 99);
    lv_obj_set_y(ui_rightLabel, LABEL_Y_SIDE);
    lv_obj_set_align(ui_rightLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_rightLabel, "GAME");
    lv_obj_set_style_text_color(ui_rightLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_rightLabel, g_font_bold_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_rightLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_leftLabel = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(ui_leftLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_leftLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_leftLabel, -99);
    lv_obj_set_y(ui_leftLabel, LABEL_Y_SIDE);
    lv_obj_set_align(ui_leftLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_leftLabel, "STORE");
    lv_obj_set_style_text_color(ui_leftLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_leftLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_leftLabel, g_font_bold_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_leftPanel = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_leftPanel, 80);
    lv_obj_set_height(ui_leftPanel, 80);
    lv_obj_set_x(ui_leftPanel, -99);
    lv_obj_set_y(ui_leftPanel, -6);
    lv_obj_set_align(ui_leftPanel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_leftPanel, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(ui_leftPanel, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_leftPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_leftPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_leftPanel, lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_leftPanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_switchPanel = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_switchPanel, 100);
    lv_obj_set_height(ui_switchPanel, 100);
    lv_obj_set_x(ui_switchPanel, 0);
    lv_obj_set_y(ui_switchPanel, -16);
    lv_obj_set_align(ui_switchPanel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_switchPanel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_switchPanel, 22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_switchPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_switchPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_switchPanel, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_switchPanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_switchPanel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_rightPanel = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_rightPanel, 80);
    lv_obj_set_height(ui_rightPanel, 80);
    lv_obj_set_x(ui_rightPanel, 99);
    lv_obj_set_y(ui_rightPanel, -6);
    lv_obj_set_align(ui_rightPanel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_rightPanel, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(ui_rightPanel, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_rightPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rightPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_rightPanel, lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_rightPanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_rightOuterPanel = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_rightOuterPanel, 61);
    lv_obj_set_height(ui_rightOuterPanel, 61);
    lv_obj_set_x(ui_rightOuterPanel, 177);
    lv_obj_set_y(ui_rightOuterPanel, 4);
    lv_obj_set_align(ui_rightOuterPanel, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_rightOuterPanel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_rightOuterPanel, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(ui_rightOuterPanel, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_rightOuterPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rightOuterPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_rightOuterPanel, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_rightOuterPanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_leftButton = lv_btn_create(::ui_APP_Container);
    lv_obj_set_width(ui_leftButton, 17);
    lv_obj_set_height(ui_leftButton, 23);
    lv_obj_set_x(ui_leftButton, -151);
    lv_obj_set_y(ui_leftButton, -4);
    lv_obj_set_align(ui_leftButton, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_leftButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_leftButton, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_leftButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_leftButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_leftButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_leftButton, ui_img_left_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_leftButton, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_leftButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_rightButton = lv_btn_create(::ui_APP_Container);
    lv_obj_set_width(ui_rightButton, 17);
    lv_obj_set_height(ui_rightButton, 23);
    lv_obj_set_x(ui_rightButton, 150);
    lv_obj_set_y(ui_rightButton, -4);
    lv_obj_set_align(ui_rightButton, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_rightButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_rightButton, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_rightButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_rightButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rightButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_rightButton, ui_img_right_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_rightButton, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_rightButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_leftOuterPanel = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(ui_leftOuterPanel, 61);
    lv_obj_set_height(ui_leftOuterPanel, 61);
    lv_obj_set_x(ui_leftOuterPanel, -177);
    lv_obj_set_y(ui_leftOuterPanel, 4);
    lv_obj_set_align(ui_leftOuterPanel, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_leftOuterPanel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_leftOuterPanel, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(ui_leftOuterPanel, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_leftOuterPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_leftOuterPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_leftOuterPanel, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_leftOuterPanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_leftOuterLabel = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(ui_leftOuterLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_leftOuterLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_leftOuterLabel, -177);
    lv_obj_set_y(ui_leftOuterLabel, LABEL_Y_SIDE);
    lv_obj_set_align(ui_leftOuterLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_leftOuterLabel, "one");
    lv_obj_add_flag(ui_leftOuterLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_set_style_text_font(ui_leftOuterLabel, g_font_bold_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_leftOuterLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_leftOuterLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_rightOuterLabel = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(ui_rightOuterLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_rightOuterLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_rightOuterLabel, 177);
    lv_obj_set_y(ui_rightOuterLabel, LABEL_Y_SIDE);
    lv_obj_set_align(ui_rightOuterLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_rightOuterLabel, "three");
    lv_obj_add_flag(ui_rightOuterLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_set_style_text_font(ui_rightOuterLabel, g_font_bold_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_rightOuterLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_rightOuterLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_leftPanel, app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_switchPanel, app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_rightPanel, app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_rightOuterPanel, app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_leftButton, switch_right, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_rightButton, switch_left, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_leftOuterPanel, app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Screen1, main_key_switch, (lv_event_code_t)LV_EVENT_KEYBOARD, NULL);


}

extern "C" void ui_Screen1_screen_init(void)
{
    UILaunchPage::create_screen();
}
