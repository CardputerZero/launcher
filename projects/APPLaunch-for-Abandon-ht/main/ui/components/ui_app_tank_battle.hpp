#pragma once
#include "ui_app_page.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class UITankBattlePage : public app_base
{
    enum class Dir { UP, DOWN, LEFT, RIGHT };

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

    static constexpr uint32_t KEY_MOVE_UP = 33;
    static constexpr uint32_t KEY_MOVE_DOWN = 45;
    static constexpr uint32_t KEY_MOVE_LEFT = 44;
    static constexpr uint32_t KEY_MOVE_RIGHT = 46;
    static constexpr uint32_t KEY_FIRE = 57;

    static constexpr int GRID_COLS = 18;
    static constexpr int GRID_ROWS = 8;
    static constexpr int CELL = 14;

    static constexpr int ARENA_X = 8;
    static constexpr int ARENA_Y = 28;
    static constexpr int ARENA_W = 304;
    static constexpr int ARENA_H = 118;

    static constexpr int GRID_W = GRID_COLS * CELL;
    static constexpr int GRID_H = GRID_ROWS * CELL;
    static constexpr int GRID_OX = ARENA_X + (ARENA_W - GRID_W) / 2;
    static constexpr int GRID_OY = ARENA_Y + (ARENA_H - GRID_H) / 2;

public:
    UITankBattlePage() : app_base()
    {
        init_game_state();
        creat_UI();
        event_handler_init();
        tick_timer_ = lv_timer_create(UITankBattlePage::static_tick_cb, 80, this);
    }

    ~UITankBattlePage()
    {
        if (tick_timer_) {
            lv_timer_del(tick_timer_);
            tick_timer_ = nullptr;
        }
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

    bool game_over_ = false;
    bool win_ = false;
    int score_ = 0;
    int tick_count_ = 0;
    std::minstd_rand rng_{0xC0FFEE};

    void init_game_state()
    {
        game_over_ = false;
        win_ = false;
        score_ = 0;
        tick_count_ = 0;

        player_ = Tank{};
        player_.x = GRID_COLS / 2;
        player_.y = GRID_ROWS - 1;
        player_.dir = Dir::UP;
        player_.alive = true;

        enemies_.clear();
        for (int i = 0; i < 5; ++i) {
            Tank e;
            e.x = 2 + i * 3;
            e.y = 0;
            e.dir = Dir::DOWN;
            e.alive = true;
            e.fire_cd = i * 4;
            enemies_.push_back(e);
        }

        bullets_.clear();
        bullets_.resize(24);
    }

    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x10151C), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        lv_obj_t *title = lv_obj_create(bg);
        lv_obj_set_size(title, 320, 24);
        lv_obj_set_pos(title, 0, 0);
        lv_obj_set_style_radius(title, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title, lv_color_hex(0x2A4D69), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title);
        lv_label_set_text(lbl_title, "Tank Demo");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title);
        lv_label_set_text(lbl_hint, "33/45/44/46 move 57 fire ESC back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0xB7D1E6), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *status = lv_label_create(bg);
        lv_label_set_text(status, "Score:0  Enemy:5");
        lv_obj_set_pos(status, 8, 24);
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

        for (int x = 1; x < GRID_COLS; ++x) {
            lv_obj_t *line = lv_obj_create(arena);
            lv_obj_set_size(line, 1, GRID_H);
            lv_obj_set_pos(line, (ARENA_W - GRID_W) / 2 + x * CELL, (ARENA_H - GRID_H) / 2);
            lv_obj_set_style_radius(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(line, lv_color_hex(0x18222D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(line, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
        }

        for (int y = 1; y < GRID_ROWS; ++y) {
            lv_obj_t *line = lv_obj_create(arena);
            lv_obj_set_size(line, GRID_W, 1);
            lv_obj_set_pos(line, (ARENA_W - GRID_W) / 2, (ARENA_H - GRID_H) / 2 + y * CELL);
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

        sync_scene();
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UITankBattlePage::static_lvgl_handler, LV_EVENT_ALL, this);
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
        if (!IS_KEY_PRESSED(e) && !IS_KEY_RELEASED(e)) {
            return;
        }

        uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
        if (key == KEY_ESC) {
            if (go_back_home) {
                go_back_home();
            }
            return;
        }

        if (!IS_KEY_PRESSED(e)) {
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

    bool inside(int x, int y) const
    {
        return (x >= 0 && x < GRID_COLS && y >= 0 && y < GRID_ROWS);
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
        case Dir::UP: dy = -1; break;
        case Dir::DOWN: dy = 1; break;
        case Dir::LEFT: dx = -1; break;
        case Dir::RIGHT: dx = 1; break;
        }
    }

    void spawn_bullet(int x, int y, Dir dir, bool from_player)
    {
        for (auto &b : bullets_) {
            if (!b.alive) {
                b.x = x;
                b.y = y;
                b.dir = dir;
                b.from_player = from_player;
                b.alive = true;
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
                if (inside(nx, ny) && !has_enemy_at(nx, ny, (int)i) && !(player_.x == nx && player_.y == ny)) {
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
                    b.alive = false;
                    game_over_ = true;
                    win_ = false;
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
            win_ = false;
            return;
        }

        if (alive_enemy == 0) {
            game_over_ = true;
            win_ = true;
        }
    }

    void place_grid_obj(lv_obj_t *obj, int gx, int gy, int w, int h)
    {
        int px = (ARENA_W - GRID_W) / 2 + gx * CELL + (CELL - w) / 2;
        int py = (ARENA_H - GRID_H) / 2 + gy * CELL + (CELL - h) / 2;
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
            auto *obj = enemy_objs_[i];
            if (enemies_[i].alive) {
                place_grid_obj(obj, enemies_[i].x, enemies_[i].y, CELL - 2, CELL - 2);
                lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            }
        }

        for (size_t i = 0; i < bullet_objs_.size() && i < bullets_.size(); ++i) {
            auto *obj = bullet_objs_[i];
            auto &b = bullets_[i];
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
            lv_snprintf(status_buf, sizeof(status_buf), "Score:%d  Enemy:%d", score_, alive_enemy_count());
        } else if (win_) {
            lv_snprintf(status_buf, sizeof(status_buf), "YOU WIN  Score:%d  Press 57 restart", score_);
        } else {
            lv_snprintf(status_buf, sizeof(status_buf), "GAME OVER  Score:%d  Press 57 restart", score_);
        }

        if (ui_obj_.count("status") && ui_obj_["status"]) {
            lv_label_set_text(ui_obj_["status"], status_buf);
        }
    }
};
