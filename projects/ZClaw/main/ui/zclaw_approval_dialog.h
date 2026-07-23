#pragma once

#include "lvgl/lvgl.h"
#include "zclaw_types.h"

namespace zclaw {

class FontManager;

class ApprovalDialog {
public:
    ApprovalDialog() = default;
    ~ApprovalDialog();

    ApprovalDialog(const ApprovalDialog &) = delete;
    ApprovalDialog &operator=(const ApprovalDialog &) = delete;

    void show(const FontManager *fonts, const ApprovalRequest &request,
              int selected_index);
    void close();
    void update_selection(int selected_index);
    bool is_open() const;

private:
    static void dialog_deleted(lv_event_t *event);
    void release_dialog();

    lv_obj_t *dialog_ = nullptr;
    lv_obj_t *buttons_[3] = {};
};

}  // namespace zclaw
