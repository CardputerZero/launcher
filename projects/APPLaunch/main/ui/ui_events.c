#include "ui.h"
#include "sample_log.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "compat/input_keys.h"


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

    leftOuterPanelToLeft_Animation(launch_circle[0], 0, NULL);
    leftPanelToCenter_Animation(launch_circle[1], 0, NULL);
    centerPanelToRight_Animation(launch_circle[2], 0, NULL);
    rightPanelToRightOuter_Animation(launch_circle[3], 0, snap_all_panels);

    snap_panel_to_slot(launch_circle[4], 0);

    lv_obj_clear_flag(launch_circle[5], LV_OBJ_FLAG_HIDDEN);

    leftOuterLabelToLeft_Animation(launch_circle[5], 0, NULL);
    leftLabelToCenter_Animation(launch_circle[6], 0, NULL);
    centerLabelToRight_Animation(launch_circle[7], 0, NULL);
    rightLabelToRightOuter_Animation(launch_circle[8], 0, NULL);

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

    rightOuterPanelToRight_Animation(launch_circle[4], 0, NULL);
    rightPanelToCenter_Animation(launch_circle[3], 0, NULL);
    centerPanelToLeft_Animation(launch_circle[2], 0, NULL);
    leftPanelToLeftOuter_Animation(launch_circle[1], 0, snap_all_panels);

    snap_panel_to_slot(launch_circle[0], 4);

    lv_obj_clear_flag(launch_circle[9], LV_OBJ_FLAG_HIDDEN);

    rightOuterLabelToRight_Animation(launch_circle[9], 0, NULL);
    rightLabelToCenter_Animation(launch_circle[8], 0, NULL);
    centerLabelToLeft_Animation(launch_circle[7], 0, NULL);
    leftLabelToLeftOuter_Animation(launch_circle[6], 0, NULL);

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
