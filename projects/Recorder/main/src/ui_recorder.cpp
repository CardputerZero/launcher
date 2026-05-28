#include "ui_recorder.h"
#include "compat/input_keys.h"

void UiRecorder::init(lv_obj_t* parent)
{
    parent_ = parent;
    buildUi(parent);
}

void UiRecorder::buildUi(lv_obj_t* parent)
{
    lv_obj_set_size(parent, 320, 170);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    lblStatus_ = lv_label_create(parent);
    lv_obj_set_pos(lblStatus_, 10, 10);
    lv_obj_set_style_text_font(lblStatus_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblStatus_, lv_color_hex(0xE0E0E0), 0);
    lv_label_set_text(lblStatus_, "IDLE");

    lblTimer_ = lv_label_create(parent);
    lv_obj_set_pos(lblTimer_, 220, 10);
    lv_obj_set_style_text_font(lblTimer_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblTimer_, lv_color_hex(0xE0E0E0), 0);
    lv_label_set_text(lblTimer_, "");

    lblFile_ = lv_label_create(parent);
    lv_obj_set_pos(lblFile_, 10, 55);
    lv_obj_set_width(lblFile_, 300);
    lv_obj_set_style_text_font(lblFile_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblFile_, lv_color_hex(0xA0A0A0), 0);
    lv_label_set_long_mode(lblFile_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(lblFile_, "No recordings");

    lblHint_ = lv_label_create(parent);
    lv_obj_set_pos(lblHint_, 10, 142);
    lv_obj_set_width(lblHint_, 300);
    lv_obj_set_style_text_font(lblHint_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblHint_, lv_color_hex(0x606060), 0);
    lv_label_set_text(lblHint_, "[Enter]Rec [P]Play [S]Stop [<- ->]File [Esc]Quit");
}

void UiRecorder::update(const RecorderState& state)
{
    if (!lblStatus_) return;
    lv_label_set_text(lblStatus_, state.statusText.c_str());
    lv_label_set_text(lblTimer_, state.timerText.c_str());
    lv_label_set_text(lblFile_, state.currentFileName.c_str());
    lv_label_set_text(lblHint_, state.hintText.c_str());
}

void UiRecorder::setActionHandler(std::function<void(const std::string&)> handler)
{
    actionHandler_ = handler;
}

void UiRecorder::onKeyPressed(uint32_t key_code)
{
    uint32_t now = lv_tick_get();
    if (key_code == lastKeyCode_ && now - lastKeyTime_ < 300) return;
    lastKeyCode_ = key_code;
    lastKeyTime_ = now;

    if (!actionHandler_) return;

    switch (key_code) {
        case KEY_ENTER:
        case KEY_KPENTER:
            actionHandler_("toggle_record");
            break;
        case KEY_P:
            actionHandler_("toggle_pause");
            break;
        case KEY_SPACE:
            actionHandler_("toggle_play");
            break;
        case KEY_S:
            actionHandler_("stop");
            break;
        case KEY_ESC:
            actionHandler_("stop");
            break;
        case KEY_LEFT:
            actionHandler_("prev_file");
            break;
        case KEY_RIGHT:
            actionHandler_("next_file");
            break;
        default:
            break;
    }
}
