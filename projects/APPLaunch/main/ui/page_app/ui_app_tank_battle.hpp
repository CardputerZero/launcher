/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "ui/ui.h"
#include "../ui_app_page.hpp"
#include "compat/input_keys.h"
#include <keyboard_input.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class UITankBattlePage : public AppPageRoot
{
private:
    enum class Dir
    {
        UP,
        DOWN,
        LEFT,
        RIGHT
    };

    struct Tank
    {
        int x = 0;
        int y = 0;
        Dir dir = Dir::UP;
        bool alive = true;
        int fire_cd = 0;
    };

    struct Bullet
    {
        int x = 0;
        int y = 0;
        Dir dir = Dir::UP;
        bool from_player = true;
        bool alive = false;
    };

private:
    static constexpr uint32_t KEY_MOVE_UP    = 33;
    static constexpr uint32_t KEY_MOVE_DOWN  = 45;
    static constexpr uint32_t KEY_MOVE_LEFT  = 44;
    static constexpr uint32_t KEY_MOVE_RIGHT = 46;
    static constexpr uint32_t KEY_FIRE       = 57;

    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 170;

    static constexpr int GRID_COLS = 18;
    static constexpr int GRID_ROWS = 8;
    static constexpr int CELL      = 14;

    static constexpr int ARENA_X = 8;
    static constexpr int ARENA_Y = 42;
    static constexpr int ARENA_W = 304;
    static constexpr int ARENA_H = 124;

    static constexpr int GRID_W  = GRID_COLS * CELL;
    static constexpr int GRID_H  = GRID_ROWS * CELL;
    static constexpr int GRID_OX = (ARENA_W - GRID_W) / 2;
    static constexpr int GRID_OY = (ARENA_H - GRID_H) / 2;

public:
    UITankBattlePage() : AppPageRoot()
    {
        init_game_state();
        creat_UI();
        event_handler_init();

        tick_timer_ = lv_timer_create(UITankBattlePage::static_tick_cb, 80, this);
    }

    ~UITankBattlePage()
    {
        delete_timer(tick_timer_);
    }

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;

    lv_timer_t *tick_timer_ = nullptr;

    Tank player_;
    std::vector<Tank> enemies_;
    std::vector<Bullet> bullets_;

    lv_obj_t *player_obj_ = nullptr;
    std::vector<lv_obj_t *> enemy_objs_;
    std::vector<lv_obj_t *> bullet_objs_;

    lv_obj_t *game_msg_panel_ = nullptr;
    lv_obj_t *game_msg_label_ = nullptr;

    bool game_over_ = false;
    bool win_       = false;

    int score_      = 0;
    int tick_count_ = 0;

    std::minstd_rand rng_{0xC0FFEE};

private:
    static void delete_timer(lv_timer_t *&timer)
    {
        if (timer) {
            lv_timer_delete(timer);
            timer = nullptr;
        }
    }

private:
    void init_game_state()
    {
        game_over_  = false;
        win_        = false;
        score_      = 0;
        tick_count_ = 0;

        player_       = Tank{};
        player_.x     = GRID_COLS / 2;
        player_.y     = GRID_ROWS - 1;
        player_.dir   = Dir::UP;
        player_.alive = true;

        enemies_.clear();

        for (int i = 0; i < 5; ++i) {
            Tank e;
            e.x       = 2 + i * 3;
            e.y       = 0;
            e.dir     = Dir::DOWN;
            e.alive   = true;
            e.fire_cd = i * 4;
            enemies_.push_back(e);
        }

        bullets_.clear();
        bullets_.resize(24);
    }

private:
    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(root_screen_);
        lv_obj_set_size(bg, SCREEN_W, SCREEN_H);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x10151C), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        lv_obj_t *title = lv_obj_create(bg);
        lv_obj_set_size(title, SCREEN_W, 24);
        lv_obj_set_pos(title, 0, 0);
        lv_obj_set_style_radius(title, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title, lv_color_hex(0x2A4D69), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(title, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title);
        lv_label_set_text(lbl_title, "Tank Game");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title);
        lv_label_set_text(lbl_hint, "33/45/44/46 move 57 fire");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0xB7D1E6), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *status = lv_label_create(bg);
        lv_label_set_text(status, "Score:0  Enemy:5");
        lv_obj_set_pos(status, 8, 26);
        lv_obj_set_width(status, 304);
        lv_label_set_long_mode(status, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(status, lv_color_hex(0xDDE6ED), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(status, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["status"] = status;

        lv_obj_t *arena = lv_obj_create(bg);
        lv_obj_set_size(arena, ARENA_W, ARENA_H);
        lv_obj_set_pos(arena, ARENA_X, ARENA_Y);
        lv_obj_set_style_radius(arena, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(arena, lv_color_hex(0x0B0F14), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(arena, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(arena, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(arena, lv_color_hex(0x2F3F4F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(arena, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(arena, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["arena"] = arena;

        for (int x = 1; x < GRID_COLS; ++x) {
            lv_obj_t *line = lv_obj_create(arena);
            lv_obj_set_size(line, 1, GRID_H);
            lv_obj_set_pos(line, GRID_OX + x * CELL, GRID_OY);
            lv_obj_set_style_radius(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(line, lv_color_hex(0x18222D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(line, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
        }

        for (int y = 1; y < GRID_ROWS; ++y) {
            lv_obj_t *line = lv_obj_create(arena);
            lv_obj_set_size(line, GRID_W, 1);
            lv_obj_set_pos(line, GRID_OX, GRID_OY + y * CELL);
            lv_obj_set_style_radius(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(line, lv_color_hex(0x18222D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(line, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
        }

        player_obj_ = lv_obj_create(arena);
        lv_obj_set_size(player_obj_, CELL - 2, CELL - 2);
        lv_obj_set_style_radius(player_obj_, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(player_obj_, lv_color_hex(0x2ECC71), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(player_obj_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(player_obj_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(player_obj_, LV_OBJ_FLAG_SCROLLABLE);

        enemy_objs_.clear();

        for (size_t i = 0; i < enemies_.size(); ++i) {
            lv_obj_t *obj = lv_obj_create(arena);
            lv_obj_set_size(obj, CELL - 2, CELL - 2);
            lv_obj_set_style_radius(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xE74C3C), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            enemy_objs_.push_back(obj);
        }

        bullet_objs_.clear();

        for (int i = 0; i < 24; ++i) {
            lv_obj_t *obj = lv_obj_create(arena);
            lv_obj_set_size(obj, 4, 4);
            lv_obj_set_style_radius(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xF4D03F), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            bullet_objs_.push_back(obj);
        }

        create_game_message_panel(arena);

        sync_scene();
    }

    void create_game_message_panel(lv_obj_t *arena)
    {
        game_msg_panel_ = lv_obj_create(arena);
        lv_obj_set_size(game_msg_panel_, 224, 66);
        lv_obj_set_pos(game_msg_panel_, (ARENA_W - 224) / 2, (ARENA_H - 66) / 2);
        lv_obj_set_style_radius(game_msg_panel_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(game_msg_panel_, lv_color_hex(0x111827), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(game_msg_panel_, 235, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(game_msg_panel_, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(game_msg_panel_, lv_color_hex(0xF4D03F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(game_msg_panel_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(game_msg_panel_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(game_msg_panel_, LV_OBJ_FLAG_HIDDEN);

        game_msg_label_ = lv_label_create(game_msg_panel_);
        lv_obj_set_width(game_msg_label_, 210);
        lv_label_set_long_mode(game_msg_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(game_msg_label_, "");
        lv_obj_set_style_text_align(game_msg_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(game_msg_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(game_msg_label_, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(game_msg_label_);
    }

private:
    void event_handler_init()
    {
        lv_obj_add_event_cb(root_screen_, UITankBattlePage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UITankBattlePage *self = static_cast<UITankBattlePage *>(lv_event_get_user_data(e));

        if (self) {
            self->event_handler(e);
        }
    }

    void event_handler(lv_event_t *e)
    {
        if (!launcher_ui::events::is_key_pressed(e) && !launcher_ui::events::is_key_released(e)) {
            return;
        }

        uint32_t key = launcher_ui::events::keyboard_key(e);

        if (key == KEY_ESC) {
            if (navigate_home) {
                navigate_home();
            }
            return;
        }

        if (!launcher_ui::events::is_key_pressed(e)) {
            return;
        }

        if (game_over_) {
            if (key == KEY_FIRE) {
                init_game_state();
                sync_scene();
            }
            return;
        }

        switch (key) {
        case KEY_MOVE_UP:
            player_.dir = Dir::UP;
            move_player(0, -1);
            break;

        case KEY_MOVE_DOWN:
            player_.dir = Dir::DOWN;
            move_player(0, 1);
            break;

        case KEY_MOVE_LEFT:
            player_.dir = Dir::LEFT;
            move_player(-1, 0);
            break;

        case KEY_MOVE_RIGHT:
            player_.dir = Dir::RIGHT;
            move_player(1, 0);
            break;

        case KEY_FIRE:
            player_fire();
            break;

        default:
            break;
        }

        sync_scene();
    }

private:
    static void static_tick_cb(lv_timer_t *t)
    {
        UITankBattlePage *self = static_cast<UITankBattlePage *>(lv_timer_get_user_data(t));

        if (self) {
            self->tick();
        }
    }

    void tick()
    {
        if (game_over_) {
            return;
        }

        ++tick_count_;

        if (player_.fire_cd > 0) {
            --player_.fire_cd;
        }

        for (auto &e : enemies_) {
            if (e.alive && e.fire_cd > 0) {
                --e.fire_cd;
            }
        }

        enemy_ai();
        move_bullets();
        check_end_state();
        sync_scene();
    }

private:
    bool inside(int x, int y) const
    {
        return x >= 0 && x < GRID_COLS && y >= 0 && y < GRID_ROWS;
    }

    bool has_enemy_at(int x, int y, int skip_idx = -1) const
    {
        for (size_t i = 0; i < enemies_.size(); ++i) {
            if ((int)i == skip_idx) {
                continue;
            }

            const auto &e = enemies_[i];

            if (e.alive && e.x == x && e.y == y) {
                return true;
            }
        }

        return false;
    }

    void move_player(int dx, int dy)
    {
        int nx = player_.x + dx;
        int ny = player_.y + dy;

        if (!inside(nx, ny)) {
            return;
        }

        if (has_enemy_at(nx, ny)) {
            return;
        }

        player_.x = nx;
        player_.y = ny;
    }

    void dir_step(Dir dir, int &dx, int &dy) const
    {
        dx = 0;
        dy = 0;

        switch (dir) {
        case Dir::UP:
            dy = -1;
            break;

        case Dir::DOWN:
            dy = 1;
            break;

        case Dir::LEFT:
            dx = -1;
            break;

        case Dir::RIGHT:
            dx = 1;
            break;
        }
    }

    void spawn_bullet(int x, int y, Dir dir, bool from_player)
    {
        for (auto &b : bullets_) {
            if (!b.alive) {
                b.x           = x;
                b.y           = y;
                b.dir         = dir;
                b.from_player = from_player;
                b.alive       = true;
                return;
            }
        }
    }

    void player_fire()
    {
        if (player_.fire_cd > 0) {
            return;
        }

        int dx = 0;
        int dy = 0;
        dir_step(player_.dir, dx, dy);

        int sx = player_.x + dx;
        int sy = player_.y + dy;

        if (!inside(sx, sy)) {
            return;
        }

        spawn_bullet(sx, sy, player_.dir, true);
        player_.fire_cd = 4;
    }

    void enemy_fire(Tank &e)
    {
        if (!e.alive || e.fire_cd > 0) {
            return;
        }

        int dx = 0;
        int dy = 0;
        dir_step(e.dir, dx, dy);

        int sx = e.x + dx;
        int sy = e.y + dy;

        if (!inside(sx, sy)) {
            return;
        }

        spawn_bullet(sx, sy, e.dir, false);
        e.fire_cd = 8 + (int)(rng_() % 8);
    }

    void enemy_ai()
    {
        for (size_t i = 0; i < enemies_.size(); ++i) {
            auto &e = enemies_[i];

            if (!e.alive) {
                continue;
            }

            if ((tick_count_ + (int)i) % 6 == 0) {
                uint32_t r = rng_() % 5;

                if (r == 0) {
                    e.dir = Dir::LEFT;
                } else if (r == 1) {
                    e.dir = Dir::RIGHT;
                } else if (r == 2) {
                    e.dir = Dir::DOWN;
                } else if (r == 3) {
                    e.dir = Dir::UP;
                }

                int dx = 0;
                int dy = 0;
                dir_step(e.dir, dx, dy);

                int nx = e.x + dx;
                int ny = e.y + dy;

                if (inside(nx, ny) &&
                    !has_enemy_at(nx, ny, (int)i) &&
                    !(player_.x == nx && player_.y == ny)) {
                    e.x = nx;
                    e.y = ny;
                }
            }

            if ((rng_() % 10) < 2) {
                enemy_fire(e);
            }
        }
    }

    void move_bullets()
    {
        for (auto &b : bullets_) {
            if (!b.alive) {
                continue;
            }

            int dx = 0;
            int dy = 0;
            dir_step(b.dir, dx, dy);

            b.x += dx;
            b.y += dy;

            if (!inside(b.x, b.y)) {
                b.alive = false;
                continue;
            }

            if (b.from_player) {
                for (auto &e : enemies_) {
                    if (e.alive && e.x == b.x && e.y == b.y) {
                        e.alive = false;
                        b.alive = false;
                        score_ += 100;
                        break;
                    }
                }
            } else {
                if (player_.alive && player_.x == b.x && player_.y == b.y) {
                    player_.alive = false;
                    b.alive       = false;
                    game_over_    = true;
                    win_          = false;
                }
            }
        }
    }

    void check_end_state()
    {
        if (game_over_) {
            return;
        }

        int alive_enemy = 0;

        for (const auto &e : enemies_) {
            if (e.alive) {
                ++alive_enemy;
            }
        }

        if (!player_.alive) {
            game_over_ = true;
            win_       = false;
            return;
        }

        if (alive_enemy == 0) {
            game_over_ = true;
            win_       = true;
        }
    }

private:
    void place_grid_obj(lv_obj_t *obj, int gx, int gy, int w, int h)
    {
        int px = GRID_OX + gx * CELL + (CELL - w) / 2;
        int py = GRID_OY + gy * CELL + (CELL - h) / 2;

        lv_obj_set_pos(obj, px, py);
    }

    int alive_enemy_count() const
    {
        int c = 0;

        for (const auto &e : enemies_) {
            if (e.alive) {
                ++c;
            }
        }

        return c;
    }

    void sync_scene()
    {
        if (player_obj_) {
            place_grid_obj(player_obj_, player_.x, player_.y, CELL - 2, CELL - 2);

            if (player_.alive) {
                lv_obj_clear_flag(player_obj_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(player_obj_, LV_OBJ_FLAG_HIDDEN);
            }
        }

        for (size_t i = 0; i < enemy_objs_.size() && i < enemies_.size(); ++i) {
            lv_obj_t *obj = enemy_objs_[i];

            if (enemies_[i].alive) {
                place_grid_obj(obj, enemies_[i].x, enemies_[i].y, CELL - 2, CELL - 2);
                lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            }
        }

        for (size_t i = 0; i < bullet_objs_.size() && i < bullets_.size(); ++i) {
            lv_obj_t *obj = bullet_objs_[i];
            auto &b       = bullets_[i];

            if (b.alive) {
                place_grid_obj(obj, b.x, b.y, 4, 4);

                lv_obj_set_style_bg_color(
                    obj,
                    b.from_player ? lv_color_hex(0xF4D03F) : lv_color_hex(0xFF8C42),
                    LV_PART_MAIN | LV_STATE_DEFAULT);

                lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            }
        }

        char status_buf[96];

        if (!game_over_) {
            lv_snprintf(status_buf,
                        sizeof(status_buf),
                        "Score:%d  Enemy:%d",
                        score_,
                        alive_enemy_count());
        } else {
            lv_snprintf(status_buf,
                        sizeof(status_buf),
                        "Score:%d  Enemy:%d",
                        score_,
                        alive_enemy_count());
        }

        auto it = ui_obj_.find("status");

        if (it != ui_obj_.end() && it->second) {
            lv_label_set_text(it->second, status_buf);
        }

        sync_game_message();
    }

    void sync_game_message()
    {
        if (!game_msg_panel_ || !game_msg_label_) {
            return;
        }

        if (!game_over_) {
            lv_obj_add_flag(game_msg_panel_, LV_OBJ_FLAG_HIDDEN);
            return;
        }

        if (win_) {
            lv_label_set_text(game_msg_label_, "YOU WIN\nPress Space restart");
            lv_obj_set_style_border_color(game_msg_panel_,
                                          lv_color_hex(0x2ECC71),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_label_set_text(game_msg_label_, "GAME OVER\nPress Space restart");
            lv_obj_set_style_border_color(game_msg_panel_,
                                          lv_color_hex(0xE74C3C),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_center(game_msg_label_);
        lv_obj_clear_flag(game_msg_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(game_msg_panel_);
    }
};