#pragma once

#include "lvgl/lvgl.h"
#include "zclaw_startup_model.h"

#include <string>

namespace zclaw {

class FontManager;

class StartupView {
public:
    StartupView() = default;
    ~StartupView();

    StartupView(const StartupView &) = delete;
    StartupView &operator=(const StartupView &) = delete;

    void show_network_check(lv_obj_t *parent, const FontManager *fonts);
    void show_offline(const std::string &error);
    void show_ready();

    StartupState state() const;

private:
    static void overlay_deleted(lv_event_t *event);
    void destroy_overlay();
    void release_overlay();

    const FontManager *fonts_ = nullptr;
    lv_obj_t *overlay_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *detail_ = nullptr;
    lv_obj_t *action_ = nullptr;
    StartupState state_ = StartupState::CheckingNetwork;
};

}  // namespace zclaw
