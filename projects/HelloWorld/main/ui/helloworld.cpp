#include "main.h"
#include "lvgl/lvgl.h"

namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 170;
constexpr int kTitleBarHeight = 30;
constexpr int kNavBarHeight = 30;

struct Palette {
    lv_color_t background;
    lv_color_t surface;
    lv_color_t bar;
    lv_color_t button;
    lv_color_t border;
    lv_color_t text;
    lv_color_t muted_text;
};

struct TemplateUiState {
    bool dark_mode = false;
    bool bold_text = false;
    bool info_visible = false;
    bool counter_page = false;
    int counter = 0;

    lv_obj_t* screen = nullptr;
    lv_obj_t* title_bar = nullptr;
    lv_obj_t* content = nullptr;
    lv_obj_t* nav_bar = nullptr;
    lv_obj_t* apple_group = nullptr;
    lv_obj_t* greeting_label = nullptr;
    lv_obj_t* info_label = nullptr;
    lv_obj_t* counter_label = nullptr;
    lv_obj_t* nav_buttons[5] = {};
    lv_obj_t* nav_labels[5] = {};
};

TemplateUiState g_ui;

Palette palette(bool dark_mode)
{
    if (dark_mode) {
        return {
            lv_color_hex(0x101214),
            lv_color_hex(0x15181c),
            lv_color_hex(0x181b1f),
            lv_color_hex(0x2a2f35),
            lv_color_hex(0x30363d),
            lv_color_hex(0xf4f4f5),
            lv_color_hex(0x6f7782),
        };
    }

    return {
        lv_color_hex(0xf8f9fa),
        lv_color_hex(0xffffff),
        lv_color_hex(0xf0f0f0),
        lv_color_hex(0xe7e7e7),
        lv_color_hex(0xd0d7de),
        lv_color_hex(0x1f2328),
        lv_color_hex(0xa8adb3),
    };
}

void style_plain_container(lv_obj_t* obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void update_template_ui()
{
    const Palette colors = palette(g_ui.dark_mode);

    lv_obj_set_style_bg_color(g_ui.screen, colors.background, 0);
    lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_ui.title_bar, colors.bar, 0);
    lv_obj_set_style_bg_opa(g_ui.title_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_ui.content, colors.surface, 0);
    lv_obj_set_style_bg_opa(g_ui.content, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_ui.nav_bar, colors.bar, 0);
    lv_obj_set_style_bg_opa(g_ui.nav_bar, LV_OPA_COVER, 0);

    lv_obj_set_style_text_color(g_ui.greeting_label, colors.text, 0);
    lv_obj_set_style_text_font(g_ui.greeting_label,
                               g_ui.bold_text ? &lv_font_montserrat_22 : &lv_font_montserrat_20,
                               0);

    lv_obj_set_style_text_color(g_ui.info_label, colors.text, 0);
    if (g_ui.info_visible) {
        lv_obj_remove_flag(g_ui.info_label, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(g_ui.info_label, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text_fmt(g_ui.counter_label, "Counter: %d", g_ui.counter);
    lv_obj_set_style_text_color(g_ui.counter_label, colors.text, 0);

    if (g_ui.counter_page) {
        lv_obj_add_flag(g_ui.apple_group, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_ui.counter_label, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_remove_flag(g_ui.apple_group, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.counter_label, LV_OBJ_FLAG_HIDDEN);
    }

    const char* apple_icons[5] = {
        LV_SYMBOL_POWER,
        "B",
        LV_SYMBOL_TINT,
        "i",
        LV_SYMBOL_RIGHT,
    };
    const char* counter_icons[5] = {
        LV_SYMBOL_POWER,
        LV_SYMBOL_MINUS,
        LV_SYMBOL_TINT,
        LV_SYMBOL_PLUS,
        LV_SYMBOL_LEFT,
    };
    const char** icons = g_ui.counter_page ? counter_icons : apple_icons;

    for (int i = 0; i < 5; ++i) {
        lv_obj_set_style_text_color(g_ui.nav_labels[i], colors.text, 0);
        lv_label_set_text(g_ui.nav_labels[i], icons[i]);
        lv_obj_center(g_ui.nav_labels[i]);
    }
}

void nav_event_cb(lv_event_t* event)
{
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));

    if (index == 0) {
        g_ui.info_visible = true;
    }
    else if (index == 1) {
        if (g_ui.counter_page) {
            if (g_ui.counter > 0) {
                --g_ui.counter;
            }
        }
        else {
            g_ui.bold_text = !g_ui.bold_text;
        }
    }
    else if (index == 2) {
        g_ui.dark_mode = !g_ui.dark_mode;
    }
    else if (index == 3) {
        if (g_ui.counter_page) {
            ++g_ui.counter;
        }
        else {
            g_ui.info_visible = !g_ui.info_visible;
        }
    }
    else if (index == 4) {
        g_ui.counter_page = !g_ui.counter_page;
    }

    update_template_ui();
}

lv_obj_t* create_centered_label(lv_obj_t* parent, const char* text, const lv_font_t* font)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

void build_template_ui()
{
    g_ui.screen = lv_screen_active();
    style_plain_container(g_ui.screen);
    lv_obj_set_size(g_ui.screen, kScreenWidth, kScreenHeight);

    g_ui.title_bar = lv_obj_create(g_ui.screen);
    style_plain_container(g_ui.title_bar);
    lv_obj_set_size(g_ui.title_bar, LV_PCT(100), kTitleBarHeight);
    lv_obj_align(g_ui.title_bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* title = create_centered_label(g_ui.title_bar, "Template App", &lv_font_montserrat_14);
    lv_obj_center(title);

    g_ui.nav_bar = lv_obj_create(g_ui.screen);
    style_plain_container(g_ui.nav_bar);
    lv_obj_set_size(g_ui.nav_bar, LV_PCT(100), kNavBarHeight);
    lv_obj_align(g_ui.nav_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(g_ui.nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_ui.nav_bar,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(g_ui.nav_bar, 8, 0);
    lv_obj_set_style_pad_right(g_ui.nav_bar, 8, 0);

    for (int i = 0; i < 5; ++i) {
        g_ui.nav_buttons[i] = lv_button_create(g_ui.nav_bar);
        style_plain_container(g_ui.nav_buttons[i]);
        lv_obj_set_size(g_ui.nav_buttons[i], 32, 22);
        lv_obj_set_style_bg_opa(g_ui.nav_buttons[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(g_ui.nav_buttons[i], 0, 0);
        lv_obj_set_style_shadow_width(g_ui.nav_buttons[i], 0, 0);
        lv_obj_set_style_pad_all(g_ui.nav_buttons[i], 0, 0);
        lv_obj_add_event_cb(g_ui.nav_buttons[i],
                            nav_event_cb,
                            LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(i)));

        g_ui.nav_labels[i] = lv_label_create(g_ui.nav_buttons[i]);
        lv_obj_set_style_text_font(g_ui.nav_labels[i], &lv_font_montserrat_20, 0);
        lv_obj_center(g_ui.nav_labels[i]);
    }

    g_ui.content = lv_obj_create(g_ui.screen);
    style_plain_container(g_ui.content);
    lv_obj_set_size(g_ui.content, LV_PCT(100), kScreenHeight - kTitleBarHeight - kNavBarHeight);
    lv_obj_align(g_ui.content, LV_ALIGN_TOP_MID, 0, kTitleBarHeight);

    g_ui.apple_group = lv_obj_create(g_ui.content);
    style_plain_container(g_ui.apple_group);
    lv_obj_set_size(g_ui.apple_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_ui.apple_group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_ui.apple_group,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(g_ui.apple_group, 8, 0);
    lv_obj_center(g_ui.apple_group);

    g_ui.greeting_label = create_centered_label(g_ui.apple_group, "Hello World", &lv_font_montserrat_20);
    g_ui.info_label = create_centered_label(g_ui.apple_group, "", &lv_font_montserrat_12);
    lv_label_set_text_fmt(g_ui.info_label,
                          "LVGL v%d.%d.%d",
                          LVGL_VERSION_MAJOR,
                          LVGL_VERSION_MINOR,
                          LVGL_VERSION_PATCH);

    g_ui.counter_label = create_centered_label(g_ui.content, "Counter: 0", &lv_font_montserrat_20);
    lv_obj_center(g_ui.counter_label);

    update_template_ui();
}

} // namespace

void ui_init()
{
    build_template_ui();
}
