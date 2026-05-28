#pragma once

#include "recorder_app.h"
#include "lvgl/lvgl.h"

class UiRecorder : public IRecorderView {
public:
    UiRecorder() = default;

    void init(lv_obj_t* parent);
    void update(const RecorderState& state) override;
    void setActionHandler(std::function<void(const std::string&)> handler) override;

    // Keyboard event from input system
    void onKeyPressed(uint32_t key_code);

private:
    void buildUi(lv_obj_t* parent);

    lv_obj_t* parent_ = nullptr;
    lv_obj_t* lblStatus_ = nullptr;
    lv_obj_t* lblTimer_ = nullptr;
    lv_obj_t* lblFile_ = nullptr;
    lv_obj_t* lblHint_ = nullptr;

    uint32_t lastKeyTime_ = 0;
    uint32_t lastKeyCode_ = 0;

    std::function<void(const std::string&)> actionHandler_;
};
