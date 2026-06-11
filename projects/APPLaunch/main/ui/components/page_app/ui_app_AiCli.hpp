#pragma once

#include "ui/ui.h"
#include "../ui_app_page.hpp"

#include <functional>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>

#include "compat/input_keys.h"
#include <keyboard_input.h>

/*
 * ============================================================
 *  UIAICliPage
 *
 *  AI Voice CLI Demo
 *  Screen: 320 x 170
 *
 *  Controls:
 *    ENTER / SPACE / click TALK : start the voice-command flow
 *    ESC                     : return from the result page to the AI dialog; otherwise return to the home screen
 *
 *  Flow:
 *    TALK -> din -> waveform + voice command
 *         -> ...processing
 *         -> 5s later
 *         -> hello world
 * ============================================================
 */
class UIAICliPage : public app_
{
public:
    UIAICliPage() : app_()
    {
        creat_UI();
        event_handler_init();
        set_idle_state();
    }

    ~UIAICliPage()
    {
        delete_timer(wave_timer_);
        delete_timer(stage_timer_);
        delete_timer(processing_timer_);
        delete_timer(toast_timer_);
    }

private:
    enum class AIState
    {
        Idle,
        Listening,
        Processing,
        Done
    };

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;

    lv_timer_t *wave_timer_       = nullptr;
    lv_timer_t *stage_timer_      = nullptr;
    lv_timer_t *processing_timer_ = nullptr;
    lv_timer_t *toast_timer_      = nullptr;

    lv_chart_series_t *wave_series_ = nullptr;

    AIState state_ = AIState::Idle;

    int processing_dot_ = 0;

    std::vector<int> wave_points_ = {
        12, 18, 30, 46, 28, 64, 38, 72,
        40, 58, 32, 68, 45, 24, 52, 36,
        70, 42, 30, 56, 35, 62, 26, 44
    };

    const char *voice_cmd_ = "print hello world";

private:
    /*
     * ============================================================
     * Basic utilities
     * ============================================================
     */
    static void delete_timer(lv_timer_t *&timer)
    {
        if (timer) {
            lv_timer_delete(timer);
            timer = nullptr;
        }
    }

    lv_obj_t *create_card(lv_obj_t *parent,
                          int x,
                          int y,
                          int w,
                          int h,
                          uint32_t bg = 0x151A27,
                          uint32_t border = 0x273044)
    {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, w, h);
        lv_obj_set_pos(card, x, y);

        lv_obj_set_style_radius(card, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(card, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(card, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(card, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        return card;
    }

    lv_obj_t *create_label(lv_obj_t *parent,
                           const char *text,
                           int x,
                           int y,
                           const lv_font_t *font,
                           uint32_t color)
    {
        lv_obj_t *lbl = lv_label_create(parent);
        lv_label_set_text(lbl, text);
        lv_obj_set_pos(lbl, x, y);

        lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);

        return lbl;
    }

    lv_obj_t *create_pill(lv_obj_t *parent,
                          int x,
                          int y,
                          int w,
                          int h,
                          uint32_t bg,
                          uint32_t border)
    {
        lv_obj_t *pill = lv_obj_create(parent);
        lv_obj_set_size(pill, w, h);
        lv_obj_set_pos(pill, x, y);

        lv_obj_set_style_radius(pill, h / 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(pill, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(pill, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(pill, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(pill, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_pad_all(pill, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

        return pill;
    }

private:
    /*
     * ============================================================
     * UI construction
     * ============================================================
     */
    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_root);
        lv_obj_set_size(bg, 320, 170);
        lv_obj_set_pos(bg, 0, 0);

        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x080D18), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(bg, lv_color_hex(0x111827), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(bg, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

        ui_obj_["bg"] = bg;

        create_header(bg);
        create_terminal_card(bg);
        create_wave_and_talk_card(bg);
        create_toast(bg);

        /*
         * Created last so the full-screen hello world stays on top
         */
        create_fullscreen_result(bg);
    }

    void create_header(lv_obj_t *parent)
    {
        /*
         * AI badge
         */
        lv_obj_t *badge = create_pill(parent, 9, 8, 28, 22, 0x17243A, 0x2E94FF);
        ui_obj_["badge"] = badge;

        lv_obj_t *badge_lbl = lv_label_create(badge);
        lv_label_set_text(badge_lbl, "AI");
        lv_obj_center(badge_lbl);
        lv_obj_set_style_text_font(badge_lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(badge_lbl, lv_color_hex(0x37D6FF), LV_PART_MAIN | LV_STATE_DEFAULT);

        create_label(parent, "AI CLI", 45, 7,
                     &lv_font_montserrat_18, 0xFFFFFF);

        create_label(parent, "voice command console", 46, 25,
                     &lv_font_montserrat_10, 0x7F8AA3);

        /*
         * Status pill
         */
        lv_obj_t *status = create_pill(parent, 227, 10, 83, 18, 0x20283A, 0x334158);
        ui_obj_["status_pill"] = status;

        lv_obj_t *status_lbl = lv_label_create(status);
        lv_label_set_text(status_lbl, "READY");
        lv_obj_center(status_lbl);
        lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xB8C2D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_status"] = status_lbl;
    }

    void create_terminal_card(lv_obj_t *parent)
    {
        lv_obj_t *card = create_card(parent, 8, 38, 304, 80, 0x101623, 0x263149);
        ui_obj_["terminal_card"] = card;

        /*
         * Top three dots that simulate a terminal window
         */
        create_dot(card, 10, 9, 0xFF5F57);
        create_dot(card, 23, 9, 0xFFBD2E);
        create_dot(card, 36, 9, 0x28C840);

        create_label(card, "VOICE INPUT", 54, 5,
                     &lv_font_montserrat_10, 0x74819B);

        lv_obj_t *prompt = create_label(card, "$", 10, 25,
                                        &lv_font_montserrat_16, 0x37D6FF);
        ui_obj_["lbl_prompt"] = prompt;

        lv_obj_t *voice = create_label(card, "press TALK and say a command", 65, 26,
                                       &lv_font_montserrat_16, 0xDDE7FF);
        lv_obj_set_width(voice, 236);
        lv_label_set_long_mode(voice, LV_LABEL_LONG_SCROLL_CIRCULAR);
        ui_obj_["lbl_voice"] = voice;

        lv_obj_t *processing = create_label(card, "ready", 10, 45,
                                            &lv_font_montserrat_16, 0x8E9AB3);
        ui_obj_["lbl_processing"] = processing;

        /*
         * Output result box
         */
        lv_obj_t *result_box = lv_obj_create(card);
        lv_obj_set_size(result_box, 284, 22);
        lv_obj_set_pos(result_box, 10, 52);

        lv_obj_set_style_radius(result_box, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(result_box, lv_color_hex(0x071B15), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(result_box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(result_box, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(result_box, lv_color_hex(0x00C985), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_pad_all(result_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(result_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(result_box, LV_OBJ_FLAG_HIDDEN);

        ui_obj_["result_box"] = result_box;

        lv_obj_t *result = create_label(result_box, "hello world", 9, 4,
                                        &lv_font_montserrat_12, 0x38D893);
        ui_obj_["lbl_result"] = result;
    }

    void create_dot(lv_obj_t *parent, int x, int y, uint32_t color)
    {
        lv_obj_t *dot = lv_obj_create(parent);
        lv_obj_set_size(dot, 7, 7);
        lv_obj_set_pos(dot, x, y);

        lv_obj_set_style_radius(dot, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(dot, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(dot, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }

    void create_wave_and_talk_card(lv_obj_t *parent)
    {
        lv_obj_t *card = create_card(parent, 8, 123, 304, 39, 0x101623, 0x263149);
        ui_obj_["bottom_card"] = card;

        create_label(card, "sound wave", 11, 5,
                     &lv_font_montserrat_10, 0x74819B);

        /*
         * Waveform
         */
        lv_obj_t *chart = lv_chart_create(card);
        lv_obj_set_size(chart, 210, 18);
        lv_obj_set_pos(chart, 11, 18);

        lv_obj_set_style_bg_opa(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
        lv_chart_set_point_count(chart, wave_points_.size());
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
        lv_chart_set_div_line_count(chart, 0, 0);

        wave_series_ = lv_chart_add_series(chart,
                                           lv_color_hex(0x37D6FF),
                                           LV_CHART_AXIS_PRIMARY_Y);

        for (uint32_t i = 0; i < wave_points_.size(); ++i) {
            lv_chart_set_value_by_id(chart, wave_series_, i, wave_points_[i]);
        }

        lv_chart_refresh(chart);
        ui_obj_["wave_chart"] = chart;

        /*
         * TALK button
         */
        lv_obj_t *btn = lv_btn_create(card);
        lv_obj_set_size(btn, 62, 27);
        lv_obj_set_pos(btn, 232, 6);

        lv_obj_set_style_radius(btn, 13, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E94FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(btn, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(btn, lv_color_hex(0x176DFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(btn, 90, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_add_event_cb(btn,
                            UIAICliPage::static_talk_btn_handler,
                            LV_EVENT_ALL,
                            this);

        ui_obj_["talk_btn"] = btn;

        lv_obj_t *btn_lbl = lv_label_create(btn);
        lv_label_set_text(btn_lbl, "TALK");
        lv_obj_center(btn_lbl);
        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_talk"] = btn_lbl;
    }

    void create_toast(lv_obj_t *parent)
    {
        lv_obj_t *toast = create_pill(parent, 135, 95, 50, 20, 0x2D220A, 0xFFBD2E);
        lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
        ui_obj_["toast"] = toast;

        lv_obj_t *lbl = lv_label_create(toast);
        lv_label_set_text(lbl, "din");
        lv_obj_center(lbl);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFDD7A), LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_toast"] = lbl;
    }

private:

    void create_fullscreen_result(lv_obj_t *parent)
    {
        lv_obj_t *screen = lv_obj_create(parent);
        lv_obj_set_size(screen, 320, 170);
        lv_obj_set_pos(screen, 0, 0);

        lv_obj_set_style_radius(screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x050A12), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x071F18), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);

        ui_obj_["fullscreen_result"] = screen;

        lv_obj_t *hello = lv_label_create(screen);
        lv_label_set_text(hello, "hello world");
        lv_obj_center(hello);
        lv_obj_set_style_text_font(hello, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(hello, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_obj_["fullscreen_hello"] = hello;

        lv_obj_t *hint = lv_label_create(screen);
        lv_label_set_text(hint, "ESC to AI CLI");
        lv_obj_set_pos(hint, 118, 137);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x74819B), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /*
     * ============================================================
     * State and flow
     * ============================================================
     */

    void set_idle_state()
    {
        state_ = AIState::Idle;

        delete_timer(wave_timer_);
        delete_timer(stage_timer_);
        delete_timer(processing_timer_);
        delete_timer(toast_timer_);

        set_status("READY", 0x20283A, 0xB8C2D8);

        lv_label_set_text(ui_obj_["lbl_prompt"], "$");
        lv_label_set_text(ui_obj_["lbl_voice"], "press TALK and say a command");
        lv_label_set_text(ui_obj_["lbl_processing"], "ready");

        lv_obj_set_style_text_font(ui_obj_["lbl_voice"],
                                   &lv_font_montserrat_16,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_color(ui_obj_["lbl_voice"],
                                    lv_color_hex(0xDDE7FF),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        if (ui_obj_.count("result_box")) {
            lv_obj_add_flag(ui_obj_["result_box"], LV_OBJ_FLAG_HIDDEN);
        }

        if (ui_obj_.count("fullscreen_result")) {
            lv_obj_add_flag(ui_obj_["fullscreen_result"], LV_OBJ_FLAG_HIDDEN);
        }

        lv_label_set_text(ui_obj_["lbl_talk"], "TALK");
        lv_obj_clear_state(ui_obj_["talk_btn"], LV_STATE_DISABLED);

        set_wave_color(0x37D6FF);
        reset_wave_low();
    }


    void start_voice_flow()
    {
        if (state_ == AIState::Listening || state_ == AIState::Processing) {
            return;
        }

        delete_timer(wave_timer_);
        delete_timer(stage_timer_);
        delete_timer(processing_timer_);
        delete_timer(toast_timer_);

        state_ = AIState::Listening;
        processing_dot_ = 0;

        if (ui_obj_.count("fullscreen_result")) {
            lv_obj_add_flag(ui_obj_["fullscreen_result"], LV_OBJ_FLAG_HIDDEN);
        }

        play_din_tone();

        set_status("LISTENING", 0x0B2A35, 0x37D6FF);

        lv_label_set_text(ui_obj_["lbl_prompt"], "voice>");
        lv_label_set_text(ui_obj_["lbl_voice"], "listening...");
        lv_label_set_text(ui_obj_["lbl_processing"], "audio wave input...");

        lv_obj_set_style_text_font(ui_obj_["lbl_voice"],
                                   &lv_font_montserrat_16,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_color(ui_obj_["lbl_voice"],
                                    lv_color_hex(0xDDE7FF),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        if (ui_obj_.count("result_box")) {
            lv_obj_add_flag(ui_obj_["result_box"], LV_OBJ_FLAG_HIDDEN);
        }

        lv_label_set_text(ui_obj_["lbl_talk"], "ON AIR");
        lv_obj_add_state(ui_obj_["talk_btn"], LV_STATE_DISABLED);

        set_wave_color(0x37D6FF);

        /*
         * Audio animation lasts 3 seconds
         */
        wave_timer_ = lv_timer_create(wave_timer_cb, 120, this);

        /*
         * After 3 seconds, show the recognized command and enter processing
         */
        stage_timer_ = lv_timer_create(stage_timer_cb, 3000, this);
    }


    void begin_processing()
    {
        state_ = AIState::Processing;
        processing_dot_ = 0;

        delete_timer(wave_timer_);

        set_status("AI", 0x2D220A, 0xFFBD2E);

        /*
         * Show the recognized command
         */
        lv_label_set_text(ui_obj_["lbl_prompt"], "ai>");
        lv_label_set_text(ui_obj_["lbl_voice"], voice_cmd_);
        lv_label_set_text(ui_obj_["lbl_processing"], "...processing");

        lv_obj_set_style_text_font(ui_obj_["lbl_prompt"],
                                   &lv_font_montserrat_16,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_obj_["lbl_voice"],
                                   &lv_font_montserrat_16,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_obj_["lbl_processing"],
                                   &lv_font_montserrat_16,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_obj_["lbl_voice"],
                                    lv_color_hex(0xFFBD2E),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_label_set_text(ui_obj_["lbl_talk"], "WAIT");

        set_wave_color(0xFFBD2E);
        reset_wave_low();

        /*
         * processing animation
         */
        processing_timer_ = lv_timer_create(processing_timer_cb, 420, this);

        /*
         * processing lasts 4 seconds
         */
        stage_timer_ = lv_timer_create(stage_timer_cb, 4000, this);
    }


    void finish_command()
    {
        state_ = AIState::Done;

        delete_timer(wave_timer_);
        delete_timer(processing_timer_);

        set_status("DONE", 0x082A1F, 0x38D893);

        lv_obj_set_style_text_font(ui_obj_["lbl_voice"],
                                   &lv_font_montserrat_10,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_color(ui_obj_["lbl_voice"],
                                    lv_color_hex(0xDDE7FF),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_label_set_text(ui_obj_["lbl_prompt"], "stdout>");
        lv_label_set_text(ui_obj_["lbl_voice"], "task completed");
        lv_label_set_text(ui_obj_["lbl_processing"], "");

        lv_label_set_text(ui_obj_["lbl_talk"], "AGAIN");
        lv_obj_clear_state(ui_obj_["talk_btn"], LV_STATE_DISABLED);

        set_wave_color(0x38D893);
        set_success_wave();

        /*
         * Show hello world full-screen
         */
        if (ui_obj_.count("fullscreen_result")) {
            lv_obj_clear_flag(ui_obj_["fullscreen_result"], LV_OBJ_FLAG_HIDDEN);
        }

        if (ui_obj_.count("fullscreen_hello")) {
            lv_label_set_text(ui_obj_["fullscreen_hello"], "hello world");
        }
    }

    void set_status(const char *text, uint32_t bg, uint32_t text_color)
    {
        lv_obj_set_style_bg_color(ui_obj_["status_pill"],
                                  lv_color_hex(bg),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_label_set_text(ui_obj_["lbl_status"], text);
        lv_obj_set_style_text_color(ui_obj_["lbl_status"],
                                    lv_color_hex(text_color),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }


    void set_wave_color(uint32_t color)
    {
        if (!ui_obj_.count("wave_chart")) return;
        if (!wave_series_) return;

        lv_obj_t *chart = ui_obj_["wave_chart"];

        lv_chart_set_series_color(chart, wave_series_, lv_color_hex(color));
        lv_chart_refresh(chart);
    }

    void reset_wave_low()
    {
        if (!ui_obj_.count("wave_chart")) return;
        if (!wave_series_) return;

        lv_obj_t *chart = ui_obj_["wave_chart"];

        for (uint32_t i = 0; i < wave_points_.size(); ++i) {
            int v = 8 + rand() % 15;
            wave_points_[i] = v;
            lv_chart_set_value_by_id(chart, wave_series_, i, v);
        }

        lv_chart_refresh(chart);
    }

    void set_success_wave()
    {
        if (!ui_obj_.count("wave_chart")) return;
        if (!wave_series_) return;

        lv_obj_t *chart = ui_obj_["wave_chart"];

        int done_wave[] = {
            18, 25, 32, 45, 62, 78, 58, 42,
            32, 48, 66, 82, 70, 54, 36, 28,
            44, 60, 72, 56, 42, 30, 24, 18
        };

        for (uint32_t i = 0; i < wave_points_.size(); ++i) {
            int v = done_wave[i % 24];
            wave_points_[i] = v;
            lv_chart_set_value_by_id(chart, wave_series_, i, v);
        }

        lv_chart_refresh(chart);
    }

    void play_din_tone()
    {
        /*
         * If the hardware has a buzzer / speaker, real audio can be connected here:
         *
         *   buzzer_play(1200, 80);
         *
         * For now, use a screen toast to represent the ding prompt sound.
         */
        show_toast("din");
    }

    void show_toast(const char *text)
    {
        if (!ui_obj_.count("toast")) return;

        delete_timer(toast_timer_);

        lv_label_set_text(ui_obj_["lbl_toast"], text);
        lv_obj_clear_flag(ui_obj_["toast"], LV_OBJ_FLAG_HIDDEN);

        toast_timer_ = lv_timer_create(toast_timer_cb, 850, this);
    }

private:
    /*
     * ============================================================
     * Timer callbacks
     * ============================================================
     */
    static void wave_timer_cb(lv_timer_t *timer)
    {
        UIAICliPage *self =
            static_cast<UIAICliPage *>(lv_timer_get_user_data(timer));

        if (self) {
            self->on_wave_timer();
        }
    }

    void on_wave_timer()
    {
        if (!ui_obj_.count("wave_chart")) return;
        if (!wave_series_) return;

        lv_obj_t *chart = ui_obj_["wave_chart"];

        /*
         * Flowing waveform: drop the first point and append a new point.
         */
        if (!wave_points_.empty()) {
            wave_points_.erase(wave_points_.begin());

            int next = 0;

            if (state_ == AIState::Listening) {
                next = 20 + rand() % 75;
            } else if (state_ == AIState::Processing) {
                next = 12 + rand() % 55;
            } else {
                next = 8 + rand() % 18;
            }

            wave_points_.push_back(next);
        }

        for (uint32_t i = 0; i < wave_points_.size(); ++i) {
            lv_chart_set_value_by_id(chart, wave_series_, i, wave_points_[i]);
        }

        lv_chart_refresh(chart);
    }

    static void stage_timer_cb(lv_timer_t *timer)
    {
        UIAICliPage *self =
            static_cast<UIAICliPage *>(lv_timer_get_user_data(timer));

        if (!self) return;

        if (self->stage_timer_ == timer) {
            self->stage_timer_ = nullptr;
        }

        lv_timer_delete(timer);

        if (self->state_ == AIState::Listening) {
            self->begin_processing();
        } else if (self->state_ == AIState::Processing) {
            self->finish_command();
        }
    }

    static void processing_timer_cb(lv_timer_t *timer)
    {
        UIAICliPage *self =
            static_cast<UIAICliPage *>(lv_timer_get_user_data(timer));

        if (self) {
            self->on_processing_timer();
        }
    }

    void on_processing_timer()
    {
        if (state_ != AIState::Processing) return;

        processing_dot_ = (processing_dot_ + 1) % 4;

        const char *text = "...processing";

        if (processing_dot_ == 0) {
            text = "...processing";
        } else if (processing_dot_ == 1) {
            text = "...processing.";
        } else if (processing_dot_ == 2) {
            text = "...processing..";
        } else {
            text = "...processing...";
        }

        lv_label_set_text(ui_obj_["lbl_processing"], text);
    }

    static void toast_timer_cb(lv_timer_t *timer)
    {
        UIAICliPage *self =
            static_cast<UIAICliPage *>(lv_timer_get_user_data(timer));

        if (!self) return;

        if (self->toast_timer_ == timer) {
            self->toast_timer_ = nullptr;
        }

        lv_timer_delete(timer);

        if (self->ui_obj_.count("toast")) {
            lv_obj_add_flag(self->ui_obj_["toast"], LV_OBJ_FLAG_HIDDEN);
        }
    }

private:
    /*
     * ============================================================
     * UI button events
     * ============================================================
     */
    static void static_talk_btn_handler(lv_event_t *e)
    {
        UIAICliPage *self =
            static_cast<UIAICliPage *>(lv_event_get_user_data(e));

        if (!self) return;

        lv_event_code_t code = lv_event_get_code(e);

        if (code == LV_EVENT_PRESSED) {
            self->start_voice_flow();
        }
    }

private:
    /*
     * ============================================================
     * Keyboard events
     * ============================================================
     */
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root,
                            UIAICliPage::static_lvgl_handler,
                            LV_EVENT_ALL,
                            this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIAICliPage *self =
            static_cast<UIAICliPage *>(lv_event_get_user_data(e));

        if (self) {
            self->event_handler(e);
        }
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e)) {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            handle_key(key);
        }
    }

    void handle_key(uint32_t key)
    {
        switch (key) {
        case KEY_ESC:
            if (state_ == AIState::Done) {
                set_idle_state();
                break;
            }

            if (go_back_home) {
                go_back_home();
            }
            break;

        case KEY_TAB:
        case KEY_SPACE:
            start_voice_flow();
            break;

        default:
            break;
        }
    }
};
