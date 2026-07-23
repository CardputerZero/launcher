#include "zclaw_input_dialog.h"

#include "zclaw_fonts.hpp"
#include "zclaw_theme.h"

namespace zclaw {
namespace {

constexpr lv_coord_t kDialogHeight = 170 * 2 / 3;

}  // namespace

InputDialog::~InputDialog()
{
    close();
    if (cursor_style_initialized_)
        lv_style_reset(&cursor_style_);
}

void InputDialog::open_chat(const FontManager *fonts)
{
    mode_ = InputMode::Chat;
    open(fonts);
    if (textarea_)
        lv_textarea_set_placeholder_text(textarea_, "Type your message...");
}

void InputDialog::open_text(const FontManager *fonts, const std::string &placeholder,
                            const std::string &initial_text, InputMode mode)
{
    mode_ = mode;
    open(fonts);
    if (!textarea_)
        return;
    lv_textarea_set_placeholder_text(textarea_, placeholder.c_str());
    lv_textarea_set_text(textarea_, initial_text.c_str());
    lv_textarea_set_cursor_pos(textarea_, LV_TEXTAREA_CURSOR_LAST);
}

void InputDialog::close()
{
    lv_obj_t *dialog = dialog_;
    if (dialog)
        lv_obj_del(dialog);
    release_dialog();
}

bool InputDialog::is_open() const
{
    return dialog_ && textarea_;
}

InputMode InputDialog::mode() const
{
    return mode_;
}

std::string InputDialog::text() const
{
    if (!textarea_)
        return "";
    const char *value = lv_textarea_get_text(textarea_);
    return value ? value : "";
}

void InputDialog::append(const char *utf8)
{
    if (textarea_ && utf8 && utf8[0])
        lv_textarea_add_text(textarea_, utf8);
}

void InputDialog::insert_newline()
{
    if (textarea_)
        lv_textarea_add_char(textarea_, '\n');
}

void InputDialog::erase_before_cursor()
{
    if (textarea_)
        lv_textarea_delete_char(textarea_);
}

void InputDialog::erase_after_cursor()
{
    if (textarea_)
        lv_textarea_delete_char_forward(textarea_);
}

void InputDialog::move_left()
{
    if (textarea_)
        lv_textarea_cursor_left(textarea_);
}

void InputDialog::move_right()
{
    if (textarea_)
        lv_textarea_cursor_right(textarea_);
}

void InputDialog::move_up()
{
    if (textarea_)
        lv_textarea_cursor_up(textarea_);
}

void InputDialog::move_down()
{
    if (textarea_)
        lv_textarea_cursor_down(textarea_);
}

void InputDialog::open(const FontManager *fonts)
{
    if (is_open() || !fonts)
        return;

    dialog_ = lv_msgbox_create(lv_layer_top());
    lv_obj_add_event_cb(dialog_, dialog_deleted, LV_EVENT_DELETE, this);
    lv_obj_set_size(dialog_, 300, kDialogHeight);
    lv_obj_align(dialog_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(dialog_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(dialog_, lv_color_hex(theme::kBar), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(dialog_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(dialog_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(dialog_, lv_color_hex(theme::kPanelLine),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(dialog_, lv_color_hex(theme::kText), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(dialog_, fonts->font_10(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(dialog_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *content = lv_msgbox_get_content(dialog_);
    lv_obj_set_style_pad_all(content, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    textarea_ = lv_textarea_create(content);
    lv_obj_set_size(textarea_, 290, kDialogHeight - 10);
    lv_textarea_set_placeholder_text(textarea_, "Type your message...");
    lv_textarea_set_one_line(textarea_, false);
    lv_textarea_set_cursor_click_pos(textarea_, false);
    lv_obj_set_style_radius(textarea_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(textarea_, lv_color_hex(theme::kPanel), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(textarea_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(textarea_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(textarea_, lv_color_hex(theme::kWhite), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(textarea_, fonts->font_10(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(textarea_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (!cursor_style_initialized_) {
        lv_style_init(&cursor_style_);
        lv_style_set_bg_opa(&cursor_style_, LV_OPA_TRANSP);
        lv_style_set_border_color(&cursor_style_, lv_color_hex(theme::kPurple));
        lv_style_set_border_side(&cursor_style_, LV_BORDER_SIDE_LEFT);
        lv_style_set_border_width(&cursor_style_, 2);
        lv_style_set_pad_left(&cursor_style_, -2);
        lv_style_set_pad_right(&cursor_style_, 0);
        cursor_style_initialized_ = true;
    }
    lv_obj_add_style(textarea_, &cursor_style_, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_add_state(textarea_, LV_STATE_FOCUSED);
    lv_obj_send_event(textarea_, LV_EVENT_FOCUSED, nullptr);
    lv_textarea_set_cursor_pos(textarea_, LV_TEXTAREA_CURSOR_LAST);
}

void InputDialog::dialog_deleted(lv_event_t *event)
{
    InputDialog *dialog =
        static_cast<InputDialog *>(lv_event_get_user_data(event));
    if (dialog)
        dialog->release_dialog();
}

void InputDialog::release_dialog()
{
    dialog_ = nullptr;
    textarea_ = nullptr;
    mode_ = InputMode::Chat;
}

}  // namespace zclaw
