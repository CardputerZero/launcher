/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "input_keys.h"
#include "keyboard_input.h"
#include "lvgl/lvgl.h"
#include <SDL2/SDL.h>
#include <cstdio>

// Only included in test builds. Observe delivery at the app's active screen,
// after SDL polling and the cp0 keyboard queue, rather than just a skin hit test.
class EmulatorInputTest
{
public:
    struct Target
    {
        SDL_Rect rect;
        uint32_t key_code;
    };

    EmulatorInputTest(SDL_Window *window, SDL_Renderer *renderer, Target target,
                      unsigned click_frame, unsigned key_frame, lv_obj_t *screen)
        : window_(window), renderer_(renderer), target_(target), click_frame_(click_frame),
          key_frame_(key_frame), screen_(screen)
    {
        if (click_frame_) {
            click_event_ = SDL_RegisterEvents(1);
            failed_ = click_event_ == static_cast<uint32_t>(-1);
        }
        if (click_frame_ || key_frame_) {
            if (screen_)
                observer_ = lv_obj_add_event_cb(screen_, observe_app_event, LV_EVENT_ALL, this);
            failed_ = failed_ || !observer_;
        }
    }

    bool observe_event(const SDL_Event &event)
    {
        if (!click_frame_ || event.type != click_event_) return false;
        click_started_ = true;
        return true;
    }

    void advance(unsigned frame)
    {
        if (failed_) return;
        if (click_frame_ && !click_queued_ && frame >= click_frame_) {
            SDL_Event marker = {};
            marker.type = click_event_;
            SDL_Event down = {};
            down.type = SDL_MOUSEBUTTONDOWN;
            down.button.windowID = SDL_GetWindowID(window_);
            down.button.button = SDL_BUTTON_LEFT;
            down.button.state = SDL_PRESSED;
            SDL_RenderLogicalToWindow(renderer_, target_.rect.x + target_.rect.w / 2.0f,
                target_.rect.y + target_.rect.h / 2.0f, &down.button.x, &down.button.y);
            SDL_Event up = down;
            up.type = SDL_MOUSEBUTTONUP;
            up.button.state = SDL_RELEASED;
            failed_ = SDL_PushEvent(&marker) != 1 ||
                      SDL_PushEvent(&down) != 1 || SDL_PushEvent(&up) != 1;
            click_queued_ = true;
        }
        if (key_frame_ && !key_queued_ && frame >= key_frame_) {
            SDL_Event down = {};
            down.type = SDL_KEYDOWN;
            down.key.windowID = SDL_GetWindowID(window_);
            down.key.state = SDL_PRESSED;
            down.key.keysym.sym = SDLK_LEFT;
            down.key.keysym.scancode = SDL_SCANCODE_LEFT;
            SDL_Event up = down;
            up.type = SDL_KEYUP;
            up.key.state = SDL_RELEASED;
            failed_ = failed_ || SDL_PushEvent(&down) != 1 || SDL_PushEvent(&up) != 1;
            key_queued_ = true;
        }
    }

    bool finish()
    {
        if (screen_ && observer_)
            lv_obj_remove_event_dsc(screen_, observer_);
        screen_ = nullptr;
        observer_ = nullptr;
        failed_ = failed_ || (click_frame_ && !click_delivered_) ||
                  (key_frame_ && !key_delivered_);
        if (click_frame_ || key_frame_)
            std::printf("[EMU_TEST] click_delivered=%d key_delivered=%d result=%s\n",
                        click_delivered_, key_delivered_, failed_ ? "fail" : "pass");
        return !failed_;
    }

private:
    static void observe_app_event(lv_event_t *event)
    {
        auto *test = static_cast<EmulatorInputTest *>(lv_event_get_user_data(event));
        if (lv_event_get_code(event) == LV_EVENT_DELETE &&
            lv_event_get_target(event) == test->screen_) {
            test->screen_ = nullptr;
            test->observer_ = nullptr;
            return;
        }
        if (lv_event_get_code(event) != static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD))
            return;
        const auto *item = static_cast<const key_item *>(lv_event_get_param(event));
        if (!item || item->key_state != KBD_KEY_PRESSED) return;
        if (test->click_started_ && item->key_code == test->target_.key_code)
            test->click_delivered_ = true;
        if (test->key_queued_ && item->key_code == KEY_LEFT)
            test->key_delivered_ = true;
    }

    SDL_Window *window_;
    SDL_Renderer *renderer_;
    Target target_;
    unsigned click_frame_, key_frame_;
    lv_obj_t *screen_;
    lv_event_dsc_t *observer_ = nullptr;
    uint32_t click_event_ = 0;
    bool click_started_ = false, click_queued_ = false, click_delivered_ = false;
    bool key_queued_ = false, key_delivered_ = false, failed_ = false;
};
