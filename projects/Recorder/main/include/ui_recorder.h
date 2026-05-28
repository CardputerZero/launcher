#pragma once

#include "lvgl/lvgl.h"

class UiRecorder {
public:
    static UiRecorder& instance();

    void init(lv_obj_t* parent);
    void update(); // call from main loop periodically

    // Keyboard handling
    void onKeyPressed(uint32_t key_code);

private:
    UiRecorder() = default;

    void buildUi(lv_obj_t* parent);
    void refreshDisplay();

    lv_obj_t* parent_ = nullptr;
    lv_obj_t* lblStatus_ = nullptr;
    lv_obj_t* lblTimer_ = nullptr;
    lv_obj_t* lblFile_ = nullptr;
    lv_obj_t* lblHint_ = nullptr;

    uint32_t lastUpdate_ = 0;
    uint32_t lastKeyTime_ = 0;
    uint32_t lastKeyCode_ = 0;
};
