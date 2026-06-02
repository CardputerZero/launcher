#include "ui_compass.h"
#include "compat/input_keys.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 170;

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
    /* 顶部状态栏 */
    createStatusBar(parent);

    /* 主内容区 */
    lv_obj_t* content = lv_obj_create(parent);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, kScreenWidth, kScreenHeight - 20);
    lv_obj_set_pos(content, 0, 20);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(content, 4, 0);
    lv_obj_set_style_pad_left(content, 4, 0);
    lv_obj_set_style_pad_right(content, 4, 0);
    lv_obj_set_style_pad_bottom(content, 4, 0);
    lv_obj_set_style_pad_row(content, 2, 0);

    /* 航向角大字 */
    lv_obj_t* yawRow = lv_obj_create(content);
    lv_obj_remove_style_all(yawRow);
    lv_obj_set_size(yawRow, kScreenWidth - 8, 30);
    lv_obj_set_flex_flow(yawRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(yawRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lblHeading_ = lv_label_create(yawRow);
    lv_label_set_text(lblHeading_, "YAW:");
    lv_obj_set_style_text_font(lblHeading_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblHeading_, lv_color_hex(0x333333), 0);

    lblYaw_ = lv_label_create(yawRow);
    lv_label_set_text(lblYaw_, "---");
    lv_obj_set_style_text_font(lblYaw_, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(lblYaw_, lv_color_hex(0xE74C3C), 0);

    /* Pitch / Roll */
    lblPitchRoll_ = lv_label_create(content);
    lv_label_set_text(lblPitchRoll_, "P:--- R:---");
    lv_obj_set_style_text_font(lblPitchRoll_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblPitchRoll_, lv_color_hex(0x666666), 0);

    /* 加速度 */
    lblAccel_ = lv_label_create(content);
    lv_label_set_text(lblAccel_, "ACC: --- --- ---");
    lv_obj_set_style_text_font(lblAccel_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblAccel_, lv_color_hex(0x333333), 0);

    /* 陀螺仪 */
    lblGyro_ = lv_label_create(content);
    lv_label_set_text(lblGyro_, "GYR: --- --- ---");
    lv_obj_set_style_text_font(lblGyro_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblGyro_, lv_color_hex(0x333333), 0);

    /* 磁力计 */
    lblMag_ = lv_label_create(content);
    lv_label_set_text(lblMag_, "MAG: --- --- ---");
    lv_obj_set_style_text_font(lblMag_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblMag_, lv_color_hex(0x333333), 0);

    /* 温度 */
    lblTemp_ = lv_label_create(content);
    lv_label_set_text(lblTemp_, "T: ---");
    lv_obj_set_style_text_font(lblTemp_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblTemp_, lv_color_hex(0x333333), 0);
}

void UiCompass::createStatusBar(lv_obj_t* parent)
{
    statusBar_ = lv_obj_create(parent);
    lv_obj_remove_style_all(statusBar_);
    lv_obj_set_size(statusBar_, kScreenWidth, 20);
    lv_obj_set_pos(statusBar_, 0, 0);
    lv_obj_set_style_bg_color(statusBar_, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_opa(statusBar_, LV_OPA_COVER, 0);

    lblStatusText_ = lv_label_create(statusBar_);
    lv_obj_set_pos(lblStatusText_, 4, 2);
    lv_label_set_text(lblStatusText_, "Compass");
    lv_obj_set_style_text_font(lblStatusText_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblStatusText_, lv_color_white(), 0);
}

void UiCompass::update(const CompassState& state)
{
    if (!parent_) return;

    char buf[128];

    if (lblStatusText_ && state.statusText != lastState_.statusText) {
        lv_label_set_text(lblStatusText_, state.statusText.c_str());
    }

    if (lblYaw_) {
        std::snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", state.yaw);
        lv_label_set_text(lblYaw_, buf);
    }

    if (lblPitchRoll_) {
        std::snprintf(buf, sizeof(buf), "P:%+.1f\xc2\xb0 R:%+.1f\xc2\xb0", state.pitch, state.roll);
        lv_label_set_text(lblPitchRoll_, buf);
    }

    if (lblAccel_) {
        std::snprintf(buf, sizeof(buf), "ACC:%+.2f %+.2f %+.2f", state.accX, state.accY, state.accZ);
        lv_label_set_text(lblAccel_, buf);
    }

    if (lblGyro_) {
        std::snprintf(buf, sizeof(buf), "GYR:%+.1f %+.1f %+.1f", state.gyrX, state.gyrY, state.gyrZ);
        lv_label_set_text(lblGyro_, buf);
    }

    if (lblMag_) {
        std::snprintf(buf, sizeof(buf), "MAG:%+.1f %+.1f %+.1f", state.magX, state.magY, state.magZ);
        lv_label_set_text(lblMag_, buf);
    }

    if (lblTemp_) {
        std::snprintf(buf, sizeof(buf), "T:%.1f\xc2\xb0" "C", state.temp);
        lv_label_set_text(lblTemp_, buf);
    }

    lastState_ = state;
}

void UiCompass::onKeyPressed(uint32_t key_code, bool isRepeat)
{
    (void)isRepeat;
    if (!actionHandler_) return;

    if (key_code == KEY_F4) {
        actionHandler_("calibrate");
    } else if (key_code == KEY_F8 || key_code == KEY_ESC) {
        actionHandler_("quit");
    }
}

void UiCompass::onCharTyped(uint32_t codepoint)
{
    (void)codepoint;
}
