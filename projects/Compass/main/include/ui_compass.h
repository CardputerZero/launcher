#pragma once

#include "compass_app.h"
#include "lvgl/lvgl.h"
#include <string>

class UiCompass : public ICompassView {
public:
    UiCompass() = default;

    void init(lv_obj_t* parent);
    void update(const CompassState& state) override;
    void setActionHandler(std::function<void(const std::string&)> handler) override;

    // Keyboard event from input system
    void onKeyPressed(uint32_t key_code, bool isRepeat = false);
    void onCharTyped(uint32_t codepoint);

private:
    void buildUi(lv_obj_t* parent);
    void createStatusBar(lv_obj_t* parent);

    lv_obj_t* parent_ = nullptr;

    // Status bar
    lv_obj_t* statusBar_ = nullptr;
    lv_obj_t* lblStatusText_ = nullptr;

    // Compass heading (large)
    lv_obj_t* lblHeading_ = nullptr;
    lv_obj_t* lblYaw_ = nullptr;

    // Sensor values
    lv_obj_t* lblAccel_ = nullptr;
    lv_obj_t* lblGyro_ = nullptr;
    lv_obj_t* lblMag_ = nullptr;
    lv_obj_t* lblTemp_ = nullptr;
    lv_obj_t* lblPitchRoll_ = nullptr;

    // State cache
    CompassState lastState_{};

    std::function<void(const std::string&)> actionHandler_;
};
