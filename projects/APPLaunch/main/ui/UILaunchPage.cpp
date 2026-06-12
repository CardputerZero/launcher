#include "UILaunchPage.h"

#include "Launch.h"
#include "lvgl/src/widgets/gif/lv_gif.h"
#include "sample_log.h"
#include "compat/input_keys.h"
#include <utility>

#include "Animation/ui_launcher_animation.h"

#include <algorithm>

std::array<lv_obj_t *, UILaunchPage::kLauncherCarouselElementCount> UILaunchPage::carousel_elements = {};

static void rotate_carousel_left(size_t start, size_t end)
{
    auto &items = UILaunchPage::carousel_elements;
    std::rotate(items.begin() + start, items.begin() + start + 1, items.begin() + end + 1);
}

static void rotate_carousel_right(size_t start, size_t end)
{
    auto &items = UILaunchPage::carousel_elements;
    std::rotate(items.begin() + start, items.begin() + end, items.begin() + end + 1);
}

namespace {

typedef void (*switch_cb_t)(lv_event_t *);

UILaunchPage *active_launch_page = nullptr;

static void switch_left(lv_event_t *e);
static void switch_right(lv_event_t *e);
static void app_launch(lv_event_t *e);
static void main_key_switch(lv_event_t *e);

// ==================== standard layout for carousel slots ====================

struct CarouselSlot {
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t width;
    lv_coord_t height;
    bool hidden;
};

static const CarouselSlot CAROUSEL_SLOTS[] = {
    {-177, 4, 61, 61, true},
    {-99, -6, 80, 80, false},
    {0, -16, 100, 100, false},
    {99, -6, 80, 80, false},
    {177, 4, 61, 61, true},
    {-177, LABEL_Y_SIDE, 0, 0, true},
    {-99, LABEL_Y_SIDE, 0, 0, false},
    {0, LABEL_Y_CENTER, 0, 0, false},
    {99, LABEL_Y_SIDE, 0, 0, false},
    {177, LABEL_Y_SIDE, 0, 0, true},
};

static bool is_animating = false;
static switch_cb_t pending_switch = NULL;

static int Panel_current_pos = 2;
static int switch_current_pos = UILaunchPage::kPageDot2;


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
// switch panel style
// ============================================================

static void switchpanleEnable(int obj_index, int enable)
{
    lv_obj_t *obj = UILaunchPage::carousel_elements[obj_index];

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
    lv_obj_t *obj = UILaunchPage::carousel_elements[obj_index];

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
    const CarouselSlot &layout = CAROUSEL_SLOTS[slot];
    lv_obj_set_x(panel, layout.x);
    lv_obj_set_y(panel, layout.y);
    lv_obj_set_width(panel, layout.width);
    lv_obj_set_height(panel, layout.height);

    if (layout.hidden)
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
    const CarouselSlot &layout = CAROUSEL_SLOTS[slot];
    lv_obj_set_x(label, layout.x);
    lv_obj_set_y(label, layout.y);

    if (layout.hidden)
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
        snap_panel_to_slot(UILaunchPage::carousel_elements[i], i);
    }

    for (int i = 5; i < 10; i++)
    {
        snap_label_to_slot(UILaunchPage::carousel_elements[i], i);
    }

    is_animating = false;

    // Reset border colors: center=bright, sides=dark
    for (int i = 0; i < 5; i++) {
        uint32_t color = (i == 2) ? BORDER_COLOR_CENTER : BORDER_COLOR_SIDE;
        lv_obj_set_style_border_color(UILaunchPage::carousel_elements[i], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Reset all label fonts to bold
    for (int i = 5; i < 10; i++) {
        lv_obj_set_style_text_font(UILaunchPage::carousel_elements[i], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
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

static void switch_right(lv_event_t *e)
{
    if (is_animating)
    {
        pending_switch = &switch_right;
        return;
    }

    is_animating = true;

    lv_obj_clear_flag(UILaunchPage::carousel_elements[0], LV_OBJ_FLAG_HIDDEN);

    launcher_home_animation::animate_right(UILaunchPage::carousel_elements.data(), snap_all_panels);

    snap_panel_to_slot(UILaunchPage::carousel_elements[4], 0);

    lv_obj_clear_flag(UILaunchPage::carousel_elements[5], LV_OBJ_FLAG_HIDDEN);

    snap_label_to_slot(UILaunchPage::carousel_elements[9], 5);

    if (active_launch_page)
        active_launch_page->update_right_slot(UILaunchPage::carousel_elements[4], UILaunchPage::carousel_elements[9]);

    switchpanleEnableClick(2, 0);
    rotate_carousel_right(0, 4);
    switchpanleEnableClick(2, 1);

    rotate_carousel_right(5, 9);

    switchpanleEnable(switch_current_pos, 0);

    switch_current_pos = switch_current_pos == UILaunchPage::kPageDot0 ? UILaunchPage::kPageDot4 : switch_current_pos - 1;

    switchpanleEnable(switch_current_pos, 1);
}


// ============================================================
// Switch left; called when the left arrow is clicked
// ============================================================

static void switch_left(lv_event_t *e)
{
    if (is_animating)
    {
        pending_switch = &switch_left;
        return;
    }

    is_animating = true;

    lv_obj_clear_flag(UILaunchPage::carousel_elements[4], LV_OBJ_FLAG_HIDDEN);

    launcher_home_animation::animate_left(UILaunchPage::carousel_elements.data(), snap_all_panels);

    snap_panel_to_slot(UILaunchPage::carousel_elements[0], 4);

    lv_obj_clear_flag(UILaunchPage::carousel_elements[9], LV_OBJ_FLAG_HIDDEN);

    snap_label_to_slot(UILaunchPage::carousel_elements[5], 9);

    if (active_launch_page)
        active_launch_page->update_left_slot(UILaunchPage::carousel_elements[0], UILaunchPage::carousel_elements[5]);

    switchpanleEnableClick(2, 0);
    rotate_carousel_left(0, 4);
    switchpanleEnableClick(2, 1);

    rotate_carousel_left(5, 9);

    switchpanleEnable(switch_current_pos, 0);

    switch_current_pos = switch_current_pos == UILaunchPage::kPageDot4 ? UILaunchPage::kPageDot0 : switch_current_pos + 1;

    switchpanleEnable(switch_current_pos, 1);
}



// ============================================================
// screen / app
// ============================================================

static void go_back_home(lv_event_t *e)
{
    lv_disp_load_scr(ui_Screen1);
    UILaunchPage::bind_home_input_group();
}


static void ui_event_Screen1(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_KEYBOARD)
    {
        main_key_switch(e);
    }
}


static void app_launch(lv_event_t *e)
{
    if (active_launch_page)
        active_launch_page->launch_selected_app();
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

static void main_key_switch(lv_event_t *e)
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


} // namespace

namespace {

char gif_path[256];
lv_group_t *home_input_group = nullptr;

} // namespace

lv_obj_t *startup_gif = nullptr;

LauncherFonts::~LauncherFonts()
{
    release();
}

lv_font_t *LauncherFonts::get(const char *ttf_name, uint16_t size, lv_freetype_font_style_t style)
{
    const std::string font_key = key(ttf_name, size, style);
    auto it = fonts_.find(font_key);
    if (it != fonts_.end()) {
        return it->second ? it->second : fallback(size);
    }

    lv_font_t *font = lv_freetype_font_create(cp0_file_path_c(ttf_name), LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                              size, style);
    fonts_[font_key] = font;
    return font ? font : fallback(size);
}

void LauncherFonts::release()
{
    for (auto &item : fonts_) {
        if (item.second) {
            lv_freetype_font_delete(item.second);
            item.second = nullptr;
        }
    }
    fonts_.clear();
}

lv_font_t *LauncherFonts::fallback(uint16_t size) const
{
    if (size >= 18) {
        return (lv_font_t *)&lv_font_montserrat_20;
    }
    if (size >= 14) {
        return (lv_font_t *)&lv_font_montserrat_14;
    }
    return (lv_font_t *)&lv_font_montserrat_12;
}

std::string LauncherFonts::key(const char *ttf_name, uint16_t size, lv_freetype_font_style_t style)
{
    return std::string(ttf_name ? ttf_name : "") + "#" + std::to_string(size) + "#" +
           std::to_string(static_cast<int>(style));
}

lv_group_t *UILaunchPage::home_input_group()
{
    if (active_launch_page)
        return active_launch_page->input_group();
    return ::home_input_group;
}

lv_obj_t *UILaunchPage::panel(size_t slot)
{
    return carousel_elements[kCardFarLeft + slot];
}

lv_obj_t *UILaunchPage::label(size_t slot)
{
    return carousel_elements[kTitleFarLeft + slot];
}

void UILaunchPage::bind_home_input_group()
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev) {
        lv_indev_set_group(indev, home_input_group());
    }
}

void UILaunchPage::init_input_group()
{
    ::home_input_group = input_group();
    bind_home_input_group();
}

void UILaunchPage::load_home_screen()
{
    SLOGI("[HOME] home_screen_load() - loading launcher home screen");
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

        if (active_launch_page)
            active_launch_page->load_home_screen();
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

UILaunchPage::UILaunchPage(std::shared_ptr<Launch> launch)
    : home_base(), launch_(std::move(launch))
{
    active_launch_page = this;
}

UILaunchPage::~UILaunchPage()
{
    if (active_launch_page == this)
        active_launch_page = nullptr;
}

void UILaunchPage::update_left_slot(lv_obj_t *panel, lv_obj_t *label)
{
    if (launch_)
        launch_->update_left_slot(panel, label);
}

void UILaunchPage::update_right_slot(lv_obj_t *panel, lv_obj_t *label)
{
    if (launch_)
        launch_->update_right_slot(panel, label);
}

void UILaunchPage::launch_selected_app()
{
    if (launch_)
        launch_->launch_app();
}

void UILaunchPage::create_screen()
{
    if (!ui_Screen1)
        ui_Screen1 = screen();
    if (!::ui_APP_Container)
        ::ui_APP_Container = content_container();

    if (carousel_elements[kCardCenter])
        return;

    create_app_container(content_container());

}


void UILaunchPage::create_app_container(lv_obj_t *parent)
{
    ::ui_APP_Container = parent;
    if (!::ui_APP_Container)
        return;

    lv_obj_set_size(::ui_APP_Container, 320, 150);
    lv_obj_clear_flag(::ui_APP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    carousel_elements[kPageDot0] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kPageDot0], 5);
    lv_obj_set_height(carousel_elements[kPageDot0], 5);
    lv_obj_set_x(carousel_elements[kPageDot0], -20);
    lv_obj_set_y(carousel_elements[kPageDot0], 70);
    lv_obj_set_align(carousel_elements[kPageDot0], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements[kPageDot0], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements[kPageDot0], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kPageDot0], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements[kPageDot0], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kPageDot0], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kPageDot0], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kPageDot1] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kPageDot1], 5);
    lv_obj_set_height(carousel_elements[kPageDot1], 5);
    lv_obj_set_x(carousel_elements[kPageDot1], -10);
    lv_obj_set_y(carousel_elements[kPageDot1], 70);
    lv_obj_set_align(carousel_elements[kPageDot1], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements[kPageDot1], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements[kPageDot1], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kPageDot1], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements[kPageDot1], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kPageDot1], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kPageDot1], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kPageDot2] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kPageDot2], 10);
    lv_obj_set_height(carousel_elements[kPageDot2], 10);
    lv_obj_set_x(carousel_elements[kPageDot2], 0);
    lv_obj_set_y(carousel_elements[kPageDot2], 70);
    lv_obj_set_align(carousel_elements[kPageDot2], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements[kPageDot2], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements[kPageDot2], lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kPageDot2], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements[kPageDot2], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kPageDot2], lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kPageDot2], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kPageDot3] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kPageDot3], 5);
    lv_obj_set_height(carousel_elements[kPageDot3], 5);
    lv_obj_set_x(carousel_elements[kPageDot3], 10);
    lv_obj_set_y(carousel_elements[kPageDot3], 70);
    lv_obj_set_align(carousel_elements[kPageDot3], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements[kPageDot3], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements[kPageDot3], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kPageDot3], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements[kPageDot3], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kPageDot3], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kPageDot3], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kPageDot4] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kPageDot4], 5);
    lv_obj_set_height(carousel_elements[kPageDot4], 5);
    lv_obj_set_x(carousel_elements[kPageDot4], 20);
    lv_obj_set_y(carousel_elements[kPageDot4], 70);
    lv_obj_set_align(carousel_elements[kPageDot4], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements[kPageDot4], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements[kPageDot4], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kPageDot4], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements[kPageDot4], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kPageDot4], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kPageDot4], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kTitleCenter] = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kTitleCenter], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements[kTitleCenter], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements[kTitleCenter], 0);
    lv_obj_set_y(carousel_elements[kTitleCenter], LABEL_Y_CENTER);
    lv_obj_set_align(carousel_elements[kTitleCenter], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements[kTitleCenter], "CLI");
    lv_obj_set_style_text_font(carousel_elements[kTitleCenter], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(carousel_elements[kTitleCenter], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements[kTitleCenter], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kTitleRight] = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kTitleRight], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements[kTitleRight], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements[kTitleRight], 99);
    lv_obj_set_y(carousel_elements[kTitleRight], LABEL_Y_SIDE);
    lv_obj_set_align(carousel_elements[kTitleRight], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements[kTitleRight], "GAME");
    lv_obj_set_style_text_color(carousel_elements[kTitleRight], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(carousel_elements[kTitleRight], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements[kTitleRight], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kTitleLeft] = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kTitleLeft], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements[kTitleLeft], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements[kTitleLeft], -99);
    lv_obj_set_y(carousel_elements[kTitleLeft], LABEL_Y_SIDE);
    lv_obj_set_align(carousel_elements[kTitleLeft], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements[kTitleLeft], "STORE");
    lv_obj_set_style_text_color(carousel_elements[kTitleLeft], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements[kTitleLeft], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(carousel_elements[kTitleLeft], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kCardLeft] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kCardLeft], 80);
    lv_obj_set_height(carousel_elements[kCardLeft], 80);
    lv_obj_set_x(carousel_elements[kCardLeft], -99);
    lv_obj_set_y(carousel_elements[kCardLeft], -6);
    lv_obj_set_align(carousel_elements[kCardLeft], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements[kCardLeft], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(carousel_elements[kCardLeft], 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements[kCardLeft], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kCardLeft], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kCardLeft], lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kCardLeft], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kCardCenter] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kCardCenter], 100);
    lv_obj_set_height(carousel_elements[kCardCenter], 100);
    lv_obj_set_x(carousel_elements[kCardCenter], 0);
    lv_obj_set_y(carousel_elements[kCardCenter], -16);
    lv_obj_set_align(carousel_elements[kCardCenter], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements[kCardCenter], LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(carousel_elements[kCardCenter], 22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements[kCardCenter], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kCardCenter], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kCardCenter], lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kCardCenter], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(carousel_elements[kCardCenter], 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kCardRight] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kCardRight], 80);
    lv_obj_set_height(carousel_elements[kCardRight], 80);
    lv_obj_set_x(carousel_elements[kCardRight], 99);
    lv_obj_set_y(carousel_elements[kCardRight], -6);
    lv_obj_set_align(carousel_elements[kCardRight], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements[kCardRight], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(carousel_elements[kCardRight], 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements[kCardRight], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kCardRight], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kCardRight], lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kCardRight], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kCardFarRight] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kCardFarRight], 61);
    lv_obj_set_height(carousel_elements[kCardFarRight], 61);
    lv_obj_set_x(carousel_elements[kCardFarRight], 177);
    lv_obj_set_y(carousel_elements[kCardFarRight], 4);
    lv_obj_set_align(carousel_elements[kCardFarRight], LV_ALIGN_CENTER);
    lv_obj_add_flag(carousel_elements[kCardFarRight], LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(carousel_elements[kCardFarRight], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(carousel_elements[kCardFarRight], 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements[kCardFarRight], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kCardFarRight], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kCardFarRight], lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kCardFarRight], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

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
    lv_obj_set_style_bg_img_src(ui_leftButton, cp0_file_path_c("carousel_left_arrow.png"), LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_obj_set_style_bg_img_src(ui_rightButton, cp0_file_path_c("carousel_right_arrow.png"), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_rightButton, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_rightButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kCardFarLeft] = lv_obj_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kCardFarLeft], 61);
    lv_obj_set_height(carousel_elements[kCardFarLeft], 61);
    lv_obj_set_x(carousel_elements[kCardFarLeft], -177);
    lv_obj_set_y(carousel_elements[kCardFarLeft], 4);
    lv_obj_set_align(carousel_elements[kCardFarLeft], LV_ALIGN_CENTER);
    lv_obj_add_flag(carousel_elements[kCardFarLeft], LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(carousel_elements[kCardFarLeft], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(carousel_elements[kCardFarLeft], 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements[kCardFarLeft], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements[kCardFarLeft], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements[kCardFarLeft], lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements[kCardFarLeft], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kTitleFarLeft] = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kTitleFarLeft], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements[kTitleFarLeft], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements[kTitleFarLeft], -177);
    lv_obj_set_y(carousel_elements[kTitleFarLeft], LABEL_Y_SIDE);
    lv_obj_set_align(carousel_elements[kTitleFarLeft], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements[kTitleFarLeft], "one");
    lv_obj_add_flag(carousel_elements[kTitleFarLeft], LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_set_style_text_font(carousel_elements[kTitleFarLeft], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(carousel_elements[kTitleFarLeft], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements[kTitleFarLeft], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements[kTitleFarRight] = lv_label_create(::ui_APP_Container);
    lv_obj_set_width(carousel_elements[kTitleFarRight], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements[kTitleFarRight], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements[kTitleFarRight], 177);
    lv_obj_set_y(carousel_elements[kTitleFarRight], LABEL_Y_SIDE);
    lv_obj_set_align(carousel_elements[kTitleFarRight], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements[kTitleFarRight], "three");
    lv_obj_add_flag(carousel_elements[kTitleFarRight], LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_set_style_text_font(carousel_elements[kTitleFarRight], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(carousel_elements[kTitleFarRight], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements[kTitleFarRight], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(carousel_elements[kCardLeft], app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(carousel_elements[kCardCenter], app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(carousel_elements[kCardRight], app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(carousel_elements[kCardFarRight], app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_leftButton, switch_right, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_rightButton, switch_left, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(carousel_elements[kCardFarLeft], app_launch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Screen1, main_key_switch, (lv_event_code_t)LV_EVENT_KEYBOARD, NULL);


}
