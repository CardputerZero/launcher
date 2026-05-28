#include "ui_recorder.h"
#include "recorder_app.h"
#include "compat/input_keys.h"

#include <cstdio>

UiRecorder& UiRecorder::instance()
{
    static UiRecorder ui;
    return ui;
}

void UiRecorder::init(lv_obj_t* parent)
{
    parent_ = parent;
    buildUi(parent);
}

void UiRecorder::buildUi(lv_obj_t* parent)
{
    lv_obj_set_size(parent, 320, 170);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    // Status label (top left)
    lblStatus_ = lv_label_create(parent);
    lv_obj_set_pos(lblStatus_, 10, 10);
    lv_obj_set_style_text_font(lblStatus_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblStatus_, lv_color_hex(0xE0E0E0), 0);
    lv_label_set_text(lblStatus_, "IDLE");

    // Timer label (top right)
    lblTimer_ = lv_label_create(parent);
    lv_obj_set_pos(lblTimer_, 220, 10);
    lv_obj_set_style_text_font(lblTimer_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblTimer_, lv_color_hex(0xE0E0E0), 0);
    lv_label_set_text(lblTimer_, "");

    // File name label (center)
    lblFile_ = lv_label_create(parent);
    lv_obj_set_pos(lblFile_, 10, 55);
    lv_obj_set_width(lblFile_, 300);
    lv_obj_set_style_text_font(lblFile_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblFile_, lv_color_hex(0xA0A0A0), 0);
    lv_label_set_long_mode(lblFile_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(lblFile_, "No recordings");

    // Hint label (bottom)
    lblHint_ = lv_label_create(parent);
    lv_obj_set_pos(lblHint_, 10, 142);
    lv_obj_set_width(lblHint_, 300);
    lv_obj_set_style_text_font(lblHint_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblHint_, lv_color_hex(0x606060), 0);
    lv_label_set_text(lblHint_, "[Enter]Rec [P]Play [S]Stop [<- ->]File [Esc]Quit");
}

void UiRecorder::update()
{
    uint32_t now = lv_tick_get();
    if (now - lastUpdate_ < 200) return;
    lastUpdate_ = now;

    RecorderApp::instance().engine().poll();
    refreshDisplay();
}

void UiRecorder::refreshDisplay()
{
    auto& app = RecorderApp::instance();
    lv_label_set_text(lblStatus_, app.statusText().c_str());
    lv_label_set_text(lblTimer_, app.timerText().c_str());
    lv_label_set_text(lblFile_, app.currentFileName().c_str());

    auto s = app.state();
    switch (s) {
        case AppState::Idle:
            lv_label_set_text(lblHint_, "[Enter]Rec [Space]Play [<- ->]File [Esc]Quit");
            break;
        case AppState::Recording:
            lv_label_set_text(lblHint_, "[P]Pause [Enter]Stop");
            break;
        case AppState::RecPaused:
            lv_label_set_text(lblHint_, "[P]Resume [Enter]Stop");
            break;
        case AppState::Playing:
            lv_label_set_text(lblHint_, "[Space]Pause [S]Stop [<- ->]File");
            break;
        case AppState::PlayPaused:
            lv_label_set_text(lblHint_, "[Space]Resume [S]Stop");
            break;
    }
}

void UiRecorder::onKeyPressed(uint32_t key_code)
{
    uint32_t now = lv_tick_get();
    if (key_code == lastKeyCode_ && now - lastKeyTime_ < 300) return;
    lastKeyCode_ = key_code;
    lastKeyTime_ = now;

    auto& app = RecorderApp::instance();

    switch (key_code) {
        case KEY_ENTER:
        case KEY_KPENTER:
            app.toggleRecord();      // Enter: start/stop recording
            break;
        case KEY_P:
            app.pauseResumeRecord(); // P: pause/resume recording
            break;
        case KEY_SPACE:
            app.togglePlay();        // Space: play/pause playback
            break;
        case KEY_S:
            app.stop();              // S: stop current operation
            break;
        case KEY_ESC:
            app.stop();              // Esc: stop and quit
            break;
        case KEY_LEFT:
            app.prevFile();
            break;
        case KEY_RIGHT:
            app.nextFile();
            break;
        default:
            break;
    }
    refreshDisplay();
}
