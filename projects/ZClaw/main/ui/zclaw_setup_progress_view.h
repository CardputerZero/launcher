#pragma once

#include "lvgl/lvgl.h"
#include "zclaw_types.h"

namespace zclaw {

class FontManager;

class SetupProgressView {
public:
    SetupProgressView() = default;
    ~SetupProgressView();

    SetupProgressView(const SetupProgressView &) = delete;
    SetupProgressView &operator=(const SetupProgressView &) = delete;

    void show(lv_obj_t *parent, const FontManager *fonts);
    void update(const SetupProgress &progress);
    void clear();
    bool is_visible() const;

private:
    static void root_deleted(lv_event_t *event);
    void release();

    lv_obj_t *root_ = nullptr;
    lv_obj_t *spinner_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *detail_label_ = nullptr;
    lv_obj_t *url_prefix_label_ = nullptr;
    lv_obj_t *url_label_ = nullptr;
    lv_obj_t *path_prefix_label_ = nullptr;
    lv_obj_t *path_label_ = nullptr;
    lv_obj_t *progress_bar_ = nullptr;
    lv_obj_t *percent_label_ = nullptr;
    lv_obj_t *speed_label_ = nullptr;
};

}  // namespace zclaw
