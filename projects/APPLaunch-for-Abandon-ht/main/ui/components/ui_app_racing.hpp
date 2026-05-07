#pragma once
#include "ui_app_page.hpp"
#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class UIRacingPage : public app_base
{
    struct Obstacle
    {
        float x = 0.0f;
        float y = 0.0f;
        bool active = false;
    };

    static constexpr uint32_t KEY_UP_CODE = 33;
    static constexpr uint32_t KEY_DOWN_CODE = 45;
    static constexpr uint32_t KEY_LEFT_CODE = 44;
    static constexpr uint32_t KEY_RIGHT_CODE = 46;
    static constexpr uint32_t KEY_ACCEL_CODE = 28;
    static constexpr uint32_t KEY_BRAKE_CODE = 57;

    static constexpr int ARENA_X = 8;
    static constexpr int ARENA_Y = 26;
    static constexpr int ARENA_W = 304;
    static constexpr int ARENA_H = 118;

    static constexpr int ROAD_MARGIN = 18;
    static constexpr int CAR_W = 14;
    static constexpr int CAR_H = 18;
    static constexpr int OBSTACLE_W = 14;
    static constexpr int OBSTACLE_H = 14;

public:
    UIRacingPage() : app_base()
    {
        init_state();
        creat_UI();
        event_handler_init();
        tick_timer_ = lv_timer_create(UIRacingPage::static_tick_cb, 40, this);
    }

    ~UIRacingPage()
    {
        if (tick_timer_) {
            lv_timer_del(tick_timer_);
            tick_timer_ = nullptr;
        }
    }

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    lv_timer_t *tick_timer_ = nullptr;

    lv_obj_t *car_obj_ = nullptr;
    std::vector<lv_obj_t *> marker_objs_;
    std::vector<lv_obj_t *> obstacle_objs_;
    std::vector<Obstacle> obstacles_;

    bool key_up_ = false;
    bool key_down_ = false;
    bool key_left_ = false;
    bool key_right_ = false;

    bool game_over_ = false;
    float speed_ = 2.2f;
    float distance_ = 0.0f;
    float player_x_ = 0.0f;
    float player_y_ = 0.0f;
    float road_center_x_ = 0.0f;
    float road_target_x_ = 0.0f;
    int tick_count_ = 0;

    std::minstd_rand rng_{0x7A51CE};

    void init_state()
    {
        key_up_ = false;
        key_down_ = false;
        key_left_ = false;
        key_right_ = false;
        game_over_ = false;
        speed_ = 2.2f;
        distance_ = 0.0f;
        tick_count_ = 0;

        player_x_ = (float)(ARENA_W / 2 - CAR_W / 2);
        player_y_ = (float)(ARENA_H - CAR_H - 8);
        road_center_x_ = (float)(ARENA_W / 2);
        road_target_x_ = road_center_x_;

        obstacles_.clear();
        obstacles_.resize(8);
    }

    void reset_round()
    {
        init_state();
        for (auto &obs : obstacles_) {
            obs.active = false;
        }
        sync_scene();
    }

    static float clampf(float v, float lo, float hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x111820), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        lv_obj_t *title = lv_obj_create(bg);
        lv_obj_set_size(title, 320, 22);
        lv_obj_set_pos(title, 0, 0);
        lv_obj_set_style_radius(title, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title, lv_color_hex(0x2A4D69), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title);
        lv_label_set_text(lbl_title, "Racing Demo");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title);
        lv_label_set_text(lbl_hint, "28 accel(release) 57 brake(press)");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0xB7D1E6), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *status = lv_label_create(bg);
        lv_label_set_text(status, "SPD 2.2  DST 0m");
        lv_obj_set_pos(status, 8, 22);
        lv_obj_set_style_text_color(status, lv_color_hex(0xDDE6ED), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(status, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["status"] = status;

        lv_obj_t *arena = lv_obj_create(bg);
        lv_obj_set_size(arena, ARENA_W, ARENA_H);
        lv_obj_set_pos(arena, ARENA_X, ARENA_Y);
        lv_obj_set_style_radius(arena, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(arena, lv_color_hex(0x0B0F14), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(arena, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(arena, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(arena, lv_color_hex(0x2F3F4F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(arena, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(arena, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["arena"] = arena;

        lv_obj_t *left_edge = lv_obj_create(arena);
        lv_obj_set_size(left_edge, 3, ARENA_H);
        lv_obj_set_pos(left_edge, ROAD_MARGIN - 2, 0);
        lv_obj_set_style_radius(left_edge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(left_edge, lv_color_hex(0xD5D8DC), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(left_edge, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(left_edge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(left_edge, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *right_edge = lv_obj_create(arena);
        lv_obj_set_size(right_edge, 3, ARENA_H);
        lv_obj_set_pos(right_edge, ARENA_W - ROAD_MARGIN, 0);
        lv_obj_set_style_radius(right_edge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(right_edge, lv_color_hex(0xD5D8DC), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(right_edge, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(right_edge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(right_edge, LV_OBJ_FLAG_SCROLLABLE);

        marker_objs_.clear();
        for (int i = 0; i < 10; ++i) {
            lv_obj_t *mk = lv_obj_create(arena);
            lv_obj_set_size(mk, 4, 10);
            lv_obj_set_pos(mk, ARENA_W / 2 - 2, i * 14);
            lv_obj_set_style_radius(mk, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(mk, lv_color_hex(0xF7F9F9), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(mk, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(mk, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(mk, LV_OBJ_FLAG_SCROLLABLE);
            marker_objs_.push_back(mk);
        }

        car_obj_ = lv_obj_create(arena);
        lv_obj_set_size(car_obj_, CAR_W, CAR_H);
        lv_obj_set_style_radius(car_obj_, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(car_obj_, lv_color_hex(0x2ECC71), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(car_obj_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(car_obj_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(car_obj_, lv_color_hex(0x1E8449), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(car_obj_, LV_OBJ_FLAG_SCROLLABLE);

        obstacle_objs_.clear();
        for (size_t i = 0; i < obstacles_.size(); ++i) {
            lv_obj_t *obs = lv_obj_create(arena);
            lv_obj_set_size(obs, OBSTACLE_W, OBSTACLE_H);
            lv_obj_set_style_radius(obs, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obs, lv_color_hex(0xE74C3C), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obs, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obs, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obs, lv_color_hex(0x922B21), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(obs, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(obs, LV_OBJ_FLAG_SCROLLABLE);
            obstacle_objs_.push_back(obs);
        }

        sync_scene();
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIRacingPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIRacingPage *self = static_cast<UIRacingPage *>(lv_event_get_user_data(e));
        if (self) {
            self->event_handler(e);
        }
    }

    void event_handler(lv_event_t *e)
    {
        if (!IS_KEY_PRESSED(e) && !IS_KEY_RELEASED(e)) {
            return;
        }

        uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);

        if (key == KEY_ESC && IS_KEY_RELEASED(e)) {
            if (go_back_home) {
                go_back_home();
            }
            return;
        }

        if (IS_KEY_PRESSED(e)) {
            switch (key) {
            case KEY_UP_CODE: key_up_ = true; break;
            case KEY_DOWN_CODE: key_down_ = true; break;
            case KEY_LEFT_CODE: key_left_ = true; break;
            case KEY_RIGHT_CODE: key_right_ = true; break;
            case KEY_BRAKE_CODE:
                speed_ = std::max(0.0f, speed_ - 0.35f);
                break;
            case KEY_ENTER:
                if (game_over_) {
                    reset_round();
                }
                break;
            default:
                break;
            }
        }

        if (IS_KEY_RELEASED(e)) {
            switch (key) {
            case KEY_UP_CODE: key_up_ = false; break;
            case KEY_DOWN_CODE: key_down_ = false; break;
            case KEY_LEFT_CODE: key_left_ = false; break;
            case KEY_RIGHT_CODE: key_right_ = false; break;
            case KEY_ACCEL_CODE:
                speed_ = std::min(6.0f, speed_ + 0.40f);
                break;
            default:
                break;
            }
        }

        sync_scene();
    }

    static void static_tick_cb(lv_timer_t *t)
    {
        UIRacingPage *self = static_cast<UIRacingPage *>(lv_timer_get_user_data(t));
        if (self) {
            self->tick();
        }
    }

    void spawn_obstacle()
    {
        for (auto &obs : obstacles_) {
            if (!obs.active) {
                float left = (float)ROAD_MARGIN + 4.0f;
                float right = (float)(ARENA_W - ROAD_MARGIN - OBSTACLE_W - 4);
                uint32_t r = rng_() % 1000;
                obs.x = left + (right - left) * ((float)r / 999.0f);
                obs.y = -OBSTACLE_H;
                obs.active = true;
                return;
            }
        }
    }

    bool is_collide(const Obstacle &obs) const
    {
        float ax1 = player_x_;
        float ay1 = player_y_;
        float ax2 = player_x_ + CAR_W;
        float ay2 = player_y_ + CAR_H;

        float bx1 = obs.x;
        float by1 = obs.y;
        float bx2 = obs.x + OBSTACLE_W;
        float by2 = obs.y + OBSTACLE_H;

        if (ax1 >= bx2 || bx1 >= ax2) return false;
        if (ay1 >= by2 || by1 >= ay2) return false;
        return true;
    }

    void tick()
    {
        if (!game_over_) {
            ++tick_count_;
            distance_ += speed_ * 0.45f;

            float mx = 0.0f;
            float my = 0.0f;
            if (key_left_) mx -= 1.0f;
            if (key_right_) mx += 1.0f;
            if (key_up_) my -= 1.0f;
            if (key_down_) my += 1.0f;

            player_x_ += mx * (1.1f + speed_ * 0.20f);
            player_y_ += my * (1.0f + speed_ * 0.10f);

            player_x_ = clampf(player_x_, (float)ROAD_MARGIN + 2.0f, (float)(ARENA_W - ROAD_MARGIN - CAR_W - 2));
            player_y_ = clampf(player_y_, 4.0f, (float)(ARENA_H - CAR_H - 4));

            if ((tick_count_ % 18) == 0) {
                spawn_obstacle();
            }

            for (auto &obs : obstacles_) {
                if (!obs.active) continue;
                obs.y += 1.6f + speed_ * 0.85f;
                if (obs.y > ARENA_H + 2) {
                    obs.active = false;
                    continue;
                }
                if (is_collide(obs)) {
                    game_over_ = true;
                }
            }

            if ((tick_count_ % 24) == 0) {
                int32_t drift = (int32_t)(rng_() % 11) - 5;
                road_target_x_ += (float)drift;
                road_target_x_ = clampf(road_target_x_, (float)(ARENA_W / 2 - 18), (float)(ARENA_W / 2 + 18));
            }
            road_center_x_ += (road_target_x_ - road_center_x_) * 0.10f;
        }

        sync_scene();
    }

    void sync_scene()
    {
        lv_obj_set_pos(car_obj_, (int)player_x_, (int)player_y_);

        for (size_t i = 0; i < marker_objs_.size(); ++i) {
            int y = (int)(((i * 14) + (int)(distance_ * 2.5f)) % (ARENA_H + 14)) - 14;
            int x = (int)road_center_x_ - 2;
            lv_obj_set_pos(marker_objs_[i], x, y);
        }

        for (size_t i = 0; i < obstacle_objs_.size() && i < obstacles_.size(); ++i) {
            if (!obstacles_[i].active) {
                lv_obj_add_flag(obstacle_objs_[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_set_pos(obstacle_objs_[i], (int)obstacles_[i].x, (int)obstacles_[i].y);
            lv_obj_clear_flag(obstacle_objs_[i], LV_OBJ_FLAG_HIDDEN);
        }

        char status_buf[96];
        if (!game_over_) {
            lv_snprintf(status_buf, sizeof(status_buf), "SPD %.1f  DST %dm", (double)speed_, (int)distance_);
        } else {
            lv_snprintf(status_buf, sizeof(status_buf), "CRASH  SPD %.1f  DST %dm  ENTER restart", (double)speed_, (int)distance_);
        }

        if (ui_obj_.count("status") && ui_obj_["status"]) {
            lv_label_set_text(ui_obj_["status"], status_buf);
        }
    }
};
