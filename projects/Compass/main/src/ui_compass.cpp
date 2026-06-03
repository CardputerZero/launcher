#include "ui_compass.h"
#include "compat/input_keys.h"

#include <cmath>
#include <cstdio>
#include <cstring>

/* LVGL image asset generated from cardputerZero_compass.png */
extern const lv_image_dsc_t img_compass_disc;

namespace {

constexpr int kScreenWidth  = 320;
constexpr int kScreenHeight = 170;

// Colors (dark theme, matching Recorder)
const lv_color_t kColorBg         = lv_color_hex(0x000000);
const lv_color_t kColorText       = lv_color_hex(0xFFFFFF);
const lv_color_t kColorTextGray   = lv_color_hex(0xAAAAAA);
const lv_color_t kColorLevelDisc  = lv_color_hex(0x222222);
const lv_color_t kColorLevelBorder= lv_color_hex(0x555555);
const lv_color_t kColorCenterDot  = lv_color_hex(0x888888);
const lv_color_t kColorIconList   = lv_color_hex(0x33CC33);
const lv_color_t kColorIconExit   = lv_color_hex(0xFF0000);

} // namespace

void UiCompass::init(lv_obj_t* parent)
{
    parent_ = parent;
    buildUi(parent);
}

void UiCompass::setActionHandler(std::function<void(const std::string&)> handler)
{
    actionHandler_ = handler;
}

void UiCompass::buildUi(lv_obj_t* parent)
{
    lv_obj_set_size(parent, kScreenW, kScreenH);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, kColorBg, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    /* ---------- 左侧 Compass 区域 ---------- */
    lblCompassTitle_ = lv_label_create(parent);
    lv_label_set_text(lblCompassTitle_, "Compass");
    lv_obj_set_style_text_font(lblCompassTitle_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblCompassTitle_, kColorText, 0);
    lv_obj_set_size(lblCompassTitle_, 160, 16);
    lv_obj_set_style_text_align(lblCompassTitle_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lblCompassTitle_, 0, kStatusH - 20);

    compassDisc_ = lv_img_create(parent);
    lv_img_set_src(compassDisc_, &img_compass_disc);
    lv_obj_set_pos(compassDisc_, 20, kStatusH + 2);
    lv_img_set_pivot(compassDisc_, kCompassImgSize / 2, kCompassImgSize / 2);

    lblYaw_ = lv_label_create(parent);
    lv_label_set_text(lblYaw_, "---");
    lv_obj_set_style_text_font(lblYaw_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblYaw_, kColorText, 0);
    lv_obj_set_size(lblYaw_, 160, 14);
    lv_obj_set_style_text_align(lblYaw_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lblYaw_, 0, kScreenH - kBottomH - 24);

    /* ---------- 右侧 IMU 区域 ---------- */
    lblImuTitle_ = lv_label_create(parent);
    lv_label_set_text(lblImuTitle_, "IMU");
    lv_obj_set_style_text_font(lblImuTitle_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblImuTitle_, kColorText, 0);
    lv_obj_set_size(lblImuTitle_, 160, 16);
    lv_obj_set_style_text_align(lblImuTitle_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lblImuTitle_, 160, kStatusH - 20);

    levelDisc_ = lv_obj_create(parent);
    lv_obj_remove_style_all(levelDisc_);
    lv_obj_set_size(levelDisc_, kDiscDia, kDiscDia);
    lv_obj_set_pos(levelDisc_, 190, kStatusH + 2);
    lv_obj_set_style_radius(levelDisc_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(levelDisc_, kColorLevelDisc, 0);
    lv_obj_set_style_bg_opa(levelDisc_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(levelDisc_, 2, 0);
    lv_obj_set_style_border_color(levelDisc_, kColorLevelBorder, 0);
    lv_obj_set_style_border_opa(levelDisc_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(levelDisc_, 0, 0);
    lv_obj_clear_flag(levelDisc_, LV_OBJ_FLAG_SCROLLABLE);

    centerDot_ = lv_obj_create(levelDisc_);
    lv_obj_remove_style_all(centerDot_);
    lv_obj_set_size(centerDot_, 8, 8);
    lv_obj_set_pos(centerDot_, 46, 46);
    lv_obj_set_style_radius(centerDot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(centerDot_, kColorCenterDot, 0);
    lv_obj_set_style_bg_opa(centerDot_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(centerDot_, LV_OBJ_FLAG_SCROLLABLE);

    levelDot_ = lv_obj_create(levelDisc_);
    lv_obj_remove_style_all(levelDot_);
    lv_obj_set_size(levelDot_, 16, 16);
    lv_obj_set_pos(levelDot_, 42, 42);
    lv_obj_set_style_radius(levelDot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(levelDot_, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_opa(levelDot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(levelDot_, 2, 0);
    lv_obj_set_style_border_color(levelDot_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(levelDot_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(levelDot_, LV_OBJ_FLAG_SCROLLABLE);

    lblAcc_ = lv_label_create(parent);
    lv_label_set_text(lblAcc_, "ACC: --- --- ---");
    lv_obj_set_style_text_font(lblAcc_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblAcc_, kColorTextGray, 0);
    lv_obj_set_size(lblAcc_, 160, 12);
    lv_obj_set_style_text_align(lblAcc_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lblAcc_, 160, kScreenH - kBottomH - 36);

    lblGyr_ = lv_label_create(parent);
    lv_label_set_text(lblGyr_, "GYR: --- --- ---");
    lv_obj_set_style_text_font(lblGyr_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblGyr_, kColorTextGray, 0);
    lv_obj_set_size(lblGyr_, 160, 12);
    lv_obj_set_style_text_align(lblGyr_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lblGyr_, 160, kScreenH - kBottomH - 24);

    createStatusBar(parent);
    createBottomBar(parent);
}

void UiCompass::createStatusBar(lv_obj_t* parent)
{
    lblStatusText_ = lv_label_create(parent);
    lv_obj_set_pos(lblStatusText_, 0, 6);
    lv_obj_set_width(lblStatusText_, kScreenW);
    lv_obj_set_style_text_font(lblStatusText_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblStatusText_, kColorText, 0);
    lv_obj_set_style_text_align(lblStatusText_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblStatusText_, "Compass");
}

void UiCompass::createBottomBar(lv_obj_t* parent)
{
    for (int i = 0; i < 5; i++) {
        lblBottomBtns_[i] = lv_label_create(parent);
        lv_obj_set_pos(lblBottomBtns_[i], i * kBtnW, kScreenH - kBottomH - 4);
        lv_obj_set_size(lblBottomBtns_[i], kBtnW, kBottomH);
        lv_obj_set_style_text_font(lblBottomBtns_[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lblBottomBtns_[i], kColorText, 0);
        lv_obj_set_style_text_align(lblBottomBtns_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lblBottomBtns_[i], "--");
        lv_obj_set_style_pad_top(lblBottomBtns_[i], 0, 0);
        lv_obj_add_flag(lblBottomBtns_[i], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    }

    for (int i = 0; i < 5; i++) {
        lblBottomIndicators_[i] = lv_label_create(parent);
        lv_obj_set_pos(lblBottomIndicators_[i], i * kBtnW, kScreenH - 12);
        lv_obj_set_size(lblBottomIndicators_[i], kBtnW, 12);
        lv_obj_set_style_text_font(lblBottomIndicators_[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lblBottomIndicators_[i], kColorText, 0);
        lv_obj_set_style_text_align(lblBottomIndicators_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lblBottomIndicators_[i], "|");
    }

    // Set icons: F4=List (calibrate), F6=Exit
    lv_obj_set_style_text_font(lblBottomBtns_[0], ui::font::AppFont::svgfont(16), 0);
    lv_obj_set_style_text_color(lblBottomBtns_[0], kColorIconList, 0);
    lv_label_set_text(lblBottomBtns_[0], ui::font::ICON_LIST);

    lv_obj_set_style_text_font(lblBottomBtns_[2], ui::font::AppFont::svgfont(16), 0);
    lv_obj_set_style_text_color(lblBottomBtns_[2], kColorIconExit, 0);
    lv_label_set_text(lblBottomBtns_[2], ui::font::ICON_EXIT);
}

void UiCompass::update(const CompassState& state)
{
    if (!parent_) return;

    char buf[128];

    /* Status bar */
    if (lblStatusText_) {
        lv_label_set_text(lblStatusText_, state.statusText.c_str());
    }

    /* 偏航角 + 方位 */
    if (lblYaw_) {
        const char* dir = "";
        float y = state.yaw;
        if (y >= 337.5f || y < 22.5f)       dir = "N";
        else if (y < 67.5f)                 dir = "NE";
        else if (y < 112.5f)                dir = "E";
        else if (y < 157.5f)                dir = "SE";
        else if (y < 202.5f)                dir = "S";
        else if (y < 247.5f)                dir = "SW";
        else if (y < 292.5f)                dir = "W";
        else                                dir = "NW";
        std::snprintf(buf, sizeof(buf), "%.0f° %s", y, dir);
        lv_label_set_text(lblYaw_, buf);
    }

    /* ACC / GYR */
    if (lblAcc_) {
        std::snprintf(buf, sizeof(buf), "ACC:%6.2f %6.2f %6.2f",
                      state.accX, state.accY, state.accZ);
        lv_label_set_text(lblAcc_, buf);
    }
    if (lblGyr_) {
        std::snprintf(buf, sizeof(buf), "GYR:%6.1f %6.1f %6.1f",
                      state.gyrX, state.gyrY, state.gyrZ);
        lv_label_set_text(lblGyr_, buf);
    }

    /* 罗盘图片反向旋转，保持真北始终朝上 */
    if (compassDisc_) {
        lv_img_set_angle(compassDisc_, -(int16_t)(state.yaw * 10.0f));
    }

    /* 水平仪圆点 */
    if (levelDot_) {
        constexpr float maxOff = 30.0f;
        /* 传感器 Y → 屏幕左右，传感器 X → 屏幕上下 */
        float dx = state.accY / 9.80665f * maxOff;
        float dy = state.accX / 9.80665f * maxOff;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > maxOff) {
            dx = dx / dist * maxOff;
            dy = dy / dist * maxOff;
        }
        int dotX = 50 + (int)dx - 8;   /* 8 = 16/2 */
        int dotY = 50 + (int)dy - 8;
        lv_obj_set_pos(levelDot_, dotX, dotY);

        /* 静止判定：三轴变化量均很小才视为稳定（防抖动，任意姿态均可） */
        bool isStable = false;
        if (lastState_.sensorReady) {
            isStable = (fabsf(state.accX - lastState_.accX) < 0.2f) &&
                       (fabsf(state.accY - lastState_.accY) < 0.2f) &&
                       (fabsf(state.accZ - lastState_.accZ) < 0.2f);
        }
        lv_color_t c = isStable ? lv_color_hex(0x2ECC71) : lv_color_hex(0xE74C3C);
        lv_obj_set_style_bg_color(levelDot_, c, 0);
    }

    lastState_ = state;
}

void UiCompass::onKeyPressed(uint32_t key_code, bool isRepeat)
{
    (void)isRepeat;
    if (!actionHandler_) return;

    if (key_code == KEY_F4) {
        actionHandler_("calibrate");
    } else if (key_code == KEY_F6 || key_code == KEY_ESC) {
        actionHandler_("quit");
    }
}

void UiCompass::onCharTyped(uint32_t codepoint)
{
    (void)codepoint;
}
