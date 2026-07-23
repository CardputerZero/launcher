#include "zclaw_approval_dialog.h"

#include "zclaw_fonts.hpp"
#include "zclaw_theme.h"
#include "zclaw_widgets.h"

namespace zclaw {

ApprovalDialog::~ApprovalDialog()
{
    close();
}

void ApprovalDialog::show(const FontManager *fonts,
                          const ApprovalRequest &request,
                          int selected_index)
{
    if (!fonts)
        return;
    close();

    dialog_ = widgets::box(lv_layer_top(), 42, 30, 236, 110, theme::kBar, 8);
    lv_obj_add_event_cb(dialog_, dialog_deleted, LV_EVENT_DELETE, this);
    lv_obj_set_style_border_width(dialog_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(dialog_, lv_color_hex(theme::kPurple),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_move_foreground(dialog_);

    widgets::box(dialog_, 0, 0, 236, 20, theme::kPanel, 8);
    widgets::label(dialog_, "Permission", 10, 5, 120, 12, fonts->font_12(), theme::kText);
    widgets::label(dialog_, "Esc", 194, 6, 28, 10, fonts->font_10(), theme::kDim,
                   LV_TEXT_ALIGN_RIGHT);
    widgets::label(dialog_, request.tool, 12, 27, 212, 14,
                   fonts->font_12(), theme::kText);
    widgets::label(dialog_, request.summary, 12, 45, 212, 30,
                   fonts->font_10(), theme::kMuted);
    widgets::box(dialog_, 0, 78, 236, 1, theme::kPanelLine);

    static constexpr const char *labels[] = {"Yes", "Always", "No"};
    static constexpr uint32_t colors[] = {theme::kOnline, theme::kPurple, theme::kDim};
    for (int index = 0; index < 3; ++index) {
        buttons_[index] = widgets::box(dialog_, 12 + index * 72, 86, 68, 17,
                                       theme::kPanel, 5);
        lv_obj_set_style_border_width(buttons_[index], 1,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        widgets::label(buttons_[index], labels[index], 0, 4, 68, 10,
                       fonts->font_10(), colors[index], LV_TEXT_ALIGN_CENTER);
    }
    update_selection(selected_index);
}

void ApprovalDialog::close()
{
    lv_obj_t *dialog = dialog_;
    if (dialog)
        lv_obj_del(dialog);
    release_dialog();
}

void ApprovalDialog::dialog_deleted(lv_event_t *event)
{
    ApprovalDialog *dialog =
        static_cast<ApprovalDialog *>(lv_event_get_user_data(event));
    if (dialog)
        dialog->release_dialog();
}

void ApprovalDialog::release_dialog()
{
    dialog_ = nullptr;
    for (lv_obj_t *&button : buttons_)
        button = nullptr;
}

void ApprovalDialog::update_selection(int selected_index)
{
    for (int index = 0; index < 3; ++index) {
        if (!buttons_[index])
            continue;
        const bool selected = index == selected_index;
        lv_obj_set_style_bg_color(buttons_[index],
                                  lv_color_hex(selected ? 0x2B2B4A : theme::kPanel),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(buttons_[index],
                                      lv_color_hex(selected ? theme::kText : theme::kPanelLine),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

bool ApprovalDialog::is_open() const
{
    return dialog_ != nullptr;
}

}  // namespace zclaw
