#pragma once

#include "compass_app.h"
#include "app_font.h"
#include "lvgl/lvgl.h"
#include <array>
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
    void createBottomBar(lv_obj_t* parent);

    // Layout constants
    static constexpr int kScreenW   = 320;
    static constexpr int kScreenH   = 170;
    static constexpr int kDiscDia   = 100;
    static constexpr int kCompassImgSize = 120;
    static constexpr int kStatusH   = 30;
    static constexpr int kBottomH   = 25;
    static constexpr int kBtnW      = kScreenW / 5; // 64

    lv_obj_t* parent_ = nullptr;

    // Status bar
    lv_obj_t* lblStatusText_ = nullptr;

    // Left panel: Compass
    lv_obj_t* lblCompassTitle_ = nullptr;
    lv_obj_t* compassDisc_ = nullptr;
    lv_obj_t* lblYaw_ = nullptr;

    // Right panel: IMU
    lv_obj_t* lblImuTitle_ = nullptr;
    lv_obj_t* levelDisc_ = nullptr;
    lv_obj_t* centerDot_ = nullptr;
    lv_obj_t* levelDot_ = nullptr;
    lv_obj_t* lblAcc_ = nullptr;
    lv_obj_t* lblGyr_ = nullptr;

    // Bottom bar (5-segment like Recorder)
    std::array<lv_obj_t*, 5> lblBottomBtns_{};
    std::array<lv_obj_t*, 5> lblBottomIndicators_{};

    // State cache
    CompassState lastState_{};

    std::function<void(const std::string&)> actionHandler_;
};
