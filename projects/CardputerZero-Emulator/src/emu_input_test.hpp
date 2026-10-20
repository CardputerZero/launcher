/* SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT */
#pragma once

// Compiled only when BUILD_TESTING is enabled. Tests report state and fail via
// the process exit status; diagnostic keyboard logs are not part of the contract.
class EmulatorInputTest {
public:
    using active_fn = int (*)(void);

    EmulatorInputTest(SDL_Window *window, SDL_Renderer *renderer, active_fn active)
        : window_(window), renderer_(renderer), active_(active),
          click_frame_(emu_test_frame("EMU_TEST_CLICK_FRAME")),
          lock_frame_(emu_test_frame("EMU_TEST_LOCK_FRAME"))
    {
        if (click_frame_) {
            click_event_ = SDL_RegisterEvents(1);
            failed_ = click_event_ == static_cast<uint32_t>(-1);
        }
        if (lock_frame_) {
            failed_ = failed_ || !active_ || !g_kbd_handler;
            root_ = lv_screen_active();
            if (root_)
                event_ = lv_obj_add_event_cb(root_, count_key,
                    static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), this);
            failed_ = failed_ || !event_;
        }
    }

    bool observe_event(const SDL_Event &event)
    {
        if (!click_frame_ || event.type != click_event_) return false;
        click_pending_ = true;
        return true;
    }

    void observe_click(int side)
    {
        if (!click_pending_) return;
        click_pending_ = false;
        click_done_ = side == 3 && g_kbd_handler;
        failed_ = failed_ || !click_done_;
        printf("[EMU_TEST] click=%s\n", click_done_ ? "pass" : "fail");
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
            const auto &next = g_side_keys[3];
            SDL_RenderLogicalToWindow(renderer_, next.x + next.w / 2.0f,
                next.y + next.h / 2.0f, &down.button.x, &down.button.y);
            SDL_Event up = down;
            up.type = SDL_MOUSEBUTTONUP;
            up.button.state = SDL_RELEASED;
            failed_ = SDL_PushEvent(&marker) != 1 ||
                      SDL_PushEvent(&down) != 1 || SDL_PushEvent(&up) != 1;
            click_queued_ = true;
        }
        if (!lock_frame_ || frame < lock_frame_) return;
        const unsigned elapsed = frame - lock_frame_;
        switch (lock_step_) {
        case 0:
            // Exercise the real lock-screen gesture without a timeout override
            // or changes to the user's persisted configuration.
            inject_sdl_key(SDLK_TAB, true);
            ++lock_step_;
            break;
        case 1:
            if (elapsed < 640) break;
            failed_ = !active_();
            delivered_ = 0;
            inject_sdl_key(SDLK_TAB, false);
            ++lock_step_;
            break;
        case 2:
            if (elapsed < 650) break;
            tap(SDLK_RIGHT); // Wake black to wallpaper.
            ++lock_step_;
            break;
        case 3:
            if (elapsed < 660) break;
            tap(SDLK_TAB); // Arm unlock.
            ++lock_step_;
            break;
        case 4:
            if (elapsed < 670) break;
            tap(SDLK_RETURN); // Confirm unlock.
            ++lock_step_;
            break;
        case 5:
            if (elapsed < 780) break;
            lock_done_ = !active_() && delivered_ == 0;
            failed_ = failed_ || !lock_done_;
            printf("[EMU_TEST] lock_screen=%s\n", lock_done_ ? "pass" : "fail");
            ++lock_step_;
            break;
        }
    }

    bool finish()
    {
        if (event_) lv_obj_remove_event_dsc(root_, event_);
        failed_ = failed_ || (click_frame_ && !click_done_) ||
                  (lock_frame_ && !lock_done_);
        if (failed_) fprintf(stderr, "[EMU_TEST] result=fail\n");
        return !failed_;
    }

private:
    static void count_key(lv_event_t *event)
    {
        auto *test = static_cast<EmulatorInputTest *>(lv_event_get_user_data(event));
        ++test->delivered_;
    }
    static void tap(SDL_Keycode key)
    {
        inject_sdl_key(key, true);
        inject_sdl_key(key, false);
    }

    SDL_Window *window_;
    SDL_Renderer *renderer_;
    active_fn active_;
    unsigned click_frame_, lock_frame_;
    uint32_t click_event_ = 0;
    lv_obj_t *root_ = nullptr;
    lv_event_dsc_t *event_ = nullptr;
    unsigned delivered_ = 0, lock_step_ = 0;
    bool click_pending_ = false, click_queued_ = false, click_done_ = false;
    bool lock_done_ = false, failed_ = false;
};
