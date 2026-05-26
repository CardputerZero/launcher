#pragma once
#include "../ui_app_page.hpp"
#include <deque>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "compat/input_keys.h"

// ============================================================
//  Snake Game  UIGamePage
//  Screen: 320 x 170  (ui_root 320x170)
//
//  Layout:
//    Title bar: 22px with "GAME - Snake" and score
//    Game area: 320 x 148 pixels (40 cols x 18 rows, 8x8 cells)
//
//  Game states:
//    READY     - press ENTER to start
//    PLAYING   - arrow keys to steer, game tick every 200ms
//    GAME_OVER - ENTER to restart, ESC to quit
//
//  Keys:
//    UP/DOWN/LEFT/RIGHT - change direction (no 180-degree reversal)
//    ENTER              - start / restart
//    ESC                - quit to home
// ============================================================
class UIGamePage : public app_
{
    // ---- Screen constants ----
    static constexpr int SCREEN_W    = 320;  // Overall screen width
    static constexpr int SCREEN_H    = 170;  // Overall screen height

    // ---- Grid constants ----
    static constexpr int CELL_SIZE   = 8;
    static constexpr int GRID_COLS   = 40;   // 320 / 8
    static constexpr int GRID_ROWS   = 18;   // 144 / 8
    static constexpr int GAME_AREA_W = 320;
    static constexpr int GAME_AREA_H = 148;  // 170 - 22
    static constexpr int TITLE_BAR_H = 22;

    // ---- Colors ----
    static constexpr uint32_t COLOR_BG         = 0x0D1117;
    static constexpr uint32_t COLOR_TITLE_BAR  = 0x1F3A5F;
    static constexpr uint32_t COLOR_ACCENT      = 0x1F6FEB;
    static constexpr uint32_t COLOR_SNAKE_BODY  = 0x2ECC71;
    static constexpr uint32_t COLOR_SNAKE_HEAD  = 0x3DFF85;
    static constexpr uint32_t COLOR_FOOD        = 0xE74C3C;
    static constexpr uint32_t COLOR_TEXT        = 0xE6EDF3;
    static constexpr uint32_t COLOR_TEXT_DIM    = 0x7EA8D8;

    // ---- Direction enum ----
    enum Direction { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

    // ---- Game state enum ----
    enum GameState { STATE_READY, STATE_PLAYING, STATE_GAME_OVER };

    // ---- Coordinate pair ----
    struct Point {
        int x, y;
        bool operator==(const Point &o) const { return x == o.x && y == o.y; }
    };

    // ---- UI objects ----
    lv_obj_t *bg_          = nullptr;
    lv_obj_t *title_bar_   = nullptr;
    lv_obj_t *score_label_ = nullptr;
    lv_obj_t *game_area_   = nullptr;
    lv_obj_t *overlay_lbl_ = nullptr;   // centered text for READY / GAME_OVER

    // ---- Game timer ----
    lv_timer_t *game_timer_ = nullptr;

    // ---- Game data ----
    std::deque<Point> snake_;
    Point             food_;
    Direction         dir_          = DIR_RIGHT;
    Direction         next_dir_     = DIR_RIGHT;
    GameState         state_        = STATE_READY;
    int               score_        = 0;

public:
    UIGamePage() : app_()
    {

        creat_UI();
        event_handler_init();
    }

    ~UIGamePage()
    {
        if (game_timer_) {
            lv_timer_delete(game_timer_);
            game_timer_ = nullptr;
        }
    }

private:
    // ==================== UI construction ====================
    void creat_UI()
    {
        // -- Background panel --
        bg_ = lv_obj_create(ui_root);
        lv_obj_set_size(bg_, SCREEN_W, SCREEN_H);
        lv_obj_set_pos(bg_, 0, 0);
        lv_obj_set_style_radius(bg_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg_, lv_color_hex(COLOR_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg_, LV_OBJ_FLAG_SCROLLABLE);

        // -- Title bar --
        title_bar_ = lv_obj_create(bg_);
        lv_obj_set_size(title_bar_, SCREEN_W, TITLE_BAR_H);
        lv_obj_set_pos(title_bar_, 0, 0);
        lv_obj_set_style_radius(title_bar_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar_, lv_color_hex(COLOR_TITLE_BAR), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar_, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title_bar_);
        lv_label_set_text(lbl_title, "GAME - Snake");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        score_label_ = lv_label_create(title_bar_);
        lv_label_set_text(score_label_, "Score: 0");
        lv_obj_set_align(score_label_, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(score_label_, -8);
        lv_obj_set_style_text_color(score_label_, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(score_label_, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        // -- Game area container --
        game_area_ = lv_obj_create(bg_);
        lv_obj_set_size(game_area_, GAME_AREA_W, GAME_AREA_H);
        lv_obj_set_pos(game_area_, 0, TITLE_BAR_H);
        lv_obj_set_style_radius(game_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(game_area_, lv_color_hex(COLOR_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(game_area_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(game_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(game_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(game_area_, LV_OBJ_FLAG_SCROLLABLE);

        // -- Show ready screen --
        show_overlay("Press OK to Start");
    }

    // ==================== Overlay text (centered) ====================
    void show_overlay(const char *text)
    {
        clear_overlay();
        overlay_lbl_ = lv_label_create(game_area_);
        lv_label_set_text(overlay_lbl_, text);
        lv_obj_set_align(overlay_lbl_, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(overlay_lbl_, lv_color_hex(COLOR_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(overlay_lbl_, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(overlay_lbl_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void clear_overlay()
    {
        if (overlay_lbl_) {
            lv_obj_del(overlay_lbl_);
            overlay_lbl_ = nullptr;
        }
    }

    // ==================== Game logic ====================
    void game_reset()
    {
        snake_.clear();
        // Start in the center of the grid, length 3, moving RIGHT
        int cx = GRID_COLS / 2;
        int cy = GRID_ROWS / 2;
        snake_.push_front({cx, cy});
        snake_.push_back({cx - 1, cy});
        snake_.push_back({cx - 2, cy});

        dir_      = DIR_RIGHT;
        next_dir_ = DIR_RIGHT;
        score_    = 0;
        update_score_label();

        spawn_food();
    }

    void game_start()
    {
        state_ = STATE_PLAYING;
        clear_overlay();
        game_reset();
        render_game();

        if (!game_timer_)
            game_timer_ = lv_timer_create(UIGamePage::static_game_tick, 200, this);
    }

    void game_over()
    {
        state_ = STATE_GAME_OVER;
        if (game_timer_) {
            lv_timer_delete(game_timer_);
            game_timer_ = nullptr;
        }

        char buf[80];
        snprintf(buf, sizeof(buf), "Game Over! Score: %d\nOK: Restart  ESC: Quit", score_);
        show_overlay(buf);
    }

    void spawn_food()
    {
        // Find a random position not occupied by the snake
        int attempts = 0;
        while (attempts < GRID_COLS * GRID_ROWS * 2) {
            Point p = { rand() % GRID_COLS, rand() % GRID_ROWS };
            bool on_snake = false;
            for (const auto &s : snake_) {
                if (s == p) { on_snake = true; break; }
            }
            if (!on_snake) {
                food_ = p;
                return;
            }
            attempts++;
        }
        // Fallback: iterate grid to find any free cell
        for (int y = 0; y < GRID_ROWS; y++) {
            for (int x = 0; x < GRID_COLS; x++) {
                Point p = {x, y};
                bool on_snake = false;
                for (const auto &s : snake_) {
                    if (s == p) { on_snake = true; break; }
                }
                if (!on_snake) {
                    food_ = p;
                    return;
                }
            }
        }
    }

    void game_tick()
    {
        if (state_ != STATE_PLAYING) return;

        // Apply queued direction
        dir_ = next_dir_;

        // Calculate new head position
        Point head = snake_.front();
        switch (dir_) {
            case DIR_UP:    head.y--; break;
            case DIR_DOWN:  head.y++; break;
            case DIR_LEFT:  head.x--; break;
            case DIR_RIGHT: head.x++; break;
        }

        // Check wall collision
        if (head.x < 0 || head.x >= GRID_COLS || head.y < 0 || head.y >= GRID_ROWS) {
            game_over();
            return;
        }

        // Check self collision
        for (const auto &s : snake_) {
            if (s == head) {
                game_over();
                return;
            }
        }

        // Move: add new head
        snake_.push_front(head);

        // Check food
        if (head == food_) {
            score_ += 10;
            update_score_label();
            spawn_food();
            // Do NOT remove tail -- snake grows
        } else {
            // Remove tail
            snake_.pop_back();
        }

        render_game();
    }

    // ==================== Rendering ====================
    void render_game()
    {
        // Clear all children of game_area (but keep overlay_lbl_ null during play)
        lv_obj_clean(game_area_);
        overlay_lbl_ = nullptr;  // cleaned by lv_obj_clean

        // Draw food
        create_cell(food_.x, food_.y, COLOR_FOOD);

        // Draw snake body (tail to head-1)
        for (size_t i = 1; i < snake_.size(); i++) {
            create_cell(snake_[i].x, snake_[i].y, COLOR_SNAKE_BODY);
        }

        // Draw snake head
        if (!snake_.empty()) {
            create_cell(snake_.front().x, snake_.front().y, COLOR_SNAKE_HEAD);
        }
    }

    void create_cell(int gx, int gy, uint32_t color)
    {
        lv_obj_t *cell = lv_obj_create(game_area_);
        lv_obj_set_size(cell, 7, 7);
        lv_obj_set_pos(cell, gx * CELL_SIZE, gy * CELL_SIZE);
        lv_obj_set_style_radius(cell, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(cell, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(cell, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(cell, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    }

    void update_score_label()
    {
        if (!score_label_) return;
        char buf[32];
        snprintf(buf, sizeof(buf), "Score: %d", score_);
        lv_label_set_text(score_label_, buf);
    }

    // ==================== Timer callback ====================
    static void static_game_tick(lv_timer_t *t)
    {
        UIGamePage *self = static_cast<UIGamePage *>(lv_timer_get_user_data(t));
        if (self) self->game_tick();
    }

    // ==================== Event handling ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIGamePage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIGamePage *self = static_cast<UIGamePage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);

            switch (state_)
            {
            case STATE_READY:
                handle_ready_key(key);
                break;
            case STATE_PLAYING:
                handle_playing_key(key);
                break;
            case STATE_GAME_OVER:
                handle_gameover_key(key);
                break;
            }
        }
    }

    // ---- READY state keys ----
    void handle_ready_key(uint32_t key)
    {
        switch (key) {
        case KEY_ENTER:
            game_start();
            break;
        case KEY_ESC:
            if (go_back_home) go_back_home();
            break;
        default:
            break;
        }
    }

    // ---- PLAYING state keys ----
    void handle_playing_key(uint32_t key)
    {
        switch (key) {
        case KEY_UP:
            if (dir_ != DIR_DOWN) next_dir_ = DIR_UP;
            break;
        case KEY_DOWN:
            if (dir_ != DIR_UP) next_dir_ = DIR_DOWN;
            break;
        case KEY_LEFT:
            if (dir_ != DIR_RIGHT) next_dir_ = DIR_LEFT;
            break;
        case KEY_RIGHT:
            if (dir_ != DIR_LEFT) next_dir_ = DIR_RIGHT;
            break;
        case KEY_ESC:
            // Pause and quit
            if (game_timer_) {
                lv_timer_delete(game_timer_);
                game_timer_ = nullptr;
            }
            state_ = STATE_READY;
            lv_obj_clean(game_area_);
            overlay_lbl_ = nullptr;
            show_overlay("Press OK to Start");
            score_ = 0;
            update_score_label();
            break;
        default:
            break;
        }
    }

    // ---- GAME_OVER state keys ----
    void handle_gameover_key(uint32_t key)
    {
        switch (key) {
        case KEY_ENTER:
            clear_overlay();
            game_start();
            break;
        case KEY_ESC:
            if (go_back_home) go_back_home();
            break;
        default:
            break;
        }
    }
};
