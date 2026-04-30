/*
 * ui_loading.cpp
 *
 * Transient "Loading..." overlay for app launches.
 *
 * Created lazily on first ui_loading_show() call as a child of
 * lv_layer_top() so it floats above any screen. Never deleted;
 * visibility is toggled via LV_OBJ_FLAG_HIDDEN. An lv_spinner sits
 * to the left of a short text label, both centered on-screen inside
 * a dark semi-transparent pill — matching the look of the hint toast
 * in ui_global_hint.cpp.
 *
 * Design notes:
 *   - Because internal page construction happens synchronously on the
 *     LVGL thread, callers should follow ui_loading_show() with
 *     lv_refr_now(NULL) to force the overlay to actually paint BEFORE
 *     the slow work begins. Otherwise LVGL would only render on the
 *     next frame, which is after the freeze.
 *   - For external (forked) apps, lv_refr_now() is already performed
 *     by launch_Exec() before hal_process_exec_blocking() — the
 *     overlay stays on-screen while the child owns the framebuffer,
 *     which is exactly the desired "something is happening" feedback.
 */

#include "ui_loading.h"
#include "ui.h"
#include "lvgl/lvgl.h"

#define LOADING_BG_COLOR    0x1F3A5F
#define LOADING_BG_OPA      LV_OPA_80
#define LOADING_TEXT_COLOR  0xFFFFFF
#define LOADING_WIDTH       200
#define LOADING_HEIGHT      40
#define LOADING_SPINNER_SZ  24

static lv_obj_t *s_loading_obj     = NULL;
static lv_obj_t *s_loading_spinner = NULL;
static lv_obj_t *s_loading_label   = NULL;

static void ensure_loading_created(void)
{
    if (s_loading_obj != NULL) return;

    lv_obj_t *parent = lv_layer_top();
    if (parent == NULL) return;

    s_loading_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(s_loading_obj);
    lv_obj_set_size(s_loading_obj, LOADING_WIDTH, LOADING_HEIGHT);
    lv_obj_center(s_loading_obj);

    lv_obj_set_style_bg_color(s_loading_obj, lv_color_hex(LOADING_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(s_loading_obj, LOADING_BG_OPA, 0);
    lv_obj_set_style_radius(s_loading_obj, 8, 0);
    lv_obj_set_style_border_width(s_loading_obj, 0, 0);
    lv_obj_set_style_pad_all(s_loading_obj, 6, 0);
    lv_obj_set_style_shadow_width(s_loading_obj, 0, 0);

    /* Purely visual — never steal focus or clicks. */
    lv_obj_clear_flag(s_loading_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_loading_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_loading_obj, LV_OBJ_FLAG_IGNORE_LAYOUT);

    /* Horizontal layout: spinner on the left, label to its right. */
    lv_obj_set_flex_flow(s_loading_obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_loading_obj,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_loading_obj, 8, 0);

    /* Spinner — 1s arc, 60 degrees long. */
    s_loading_spinner = lv_spinner_create(s_loading_obj);
    lv_obj_set_size(s_loading_spinner, LOADING_SPINNER_SZ, LOADING_SPINNER_SZ);
    lv_spinner_set_anim_params(s_loading_spinner, 1000, 60);
    lv_obj_set_style_arc_width(s_loading_spinner, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_loading_spinner, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_loading_spinner,
                               lv_color_hex(0x3B6FA0), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_loading_spinner,
                               lv_color_hex(LOADING_TEXT_COLOR),
                               LV_PART_INDICATOR);

    s_loading_label = lv_label_create(s_loading_obj);
    lv_obj_set_style_text_color(s_loading_label,
                                lv_color_hex(LOADING_TEXT_COLOR), 0);
    /* Use the project's 14pt font with montserrat fallback — matches
     * the sizing of the existing hint overlay's copy. */
    lv_font_t *font = g_font_cn_14 ? g_font_cn_14
                                   : (lv_font_t *)&lv_font_montserrat_14;
    lv_obj_set_style_text_font(s_loading_label, font, 0);
    lv_label_set_text(s_loading_label, "");

    lv_obj_add_flag(s_loading_obj, LV_OBJ_FLAG_HIDDEN);
}

extern "C" void ui_loading_show(const char *label)
{
    ensure_loading_created();
    if (s_loading_obj == NULL || s_loading_label == NULL) return;

    lv_label_set_text(s_loading_label,
                      (label && label[0]) ? label : "Loading...");
    /* Make sure the overlay stays on top of any other lv_layer_top
     * children (e.g. the ui_global_hint toast). */
    lv_obj_move_foreground(s_loading_obj);
    lv_obj_clear_flag(s_loading_obj, LV_OBJ_FLAG_HIDDEN);
}

extern "C" void ui_loading_hide(void)
{
    if (s_loading_obj == NULL) return;
    lv_obj_add_flag(s_loading_obj, LV_OBJ_FLAG_HIDDEN);
}
