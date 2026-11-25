/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace cp0_esc_exit_policy {

inline constexpr std::uint32_t HINT_DELAY_MS = 500;
inline constexpr std::uint32_t TERMINATE_DELAY_MS = 3000;
inline constexpr std::uint32_t KILL_GRACE_MS = 3000;

struct Decision
{
    bool show_hint = false;
    bool hide_hint = false;
    bool send_terminate = false;
    bool send_kill = false;
};

class StateMachine
{
public:
    Decision update(std::uint64_t now_ms, bool esc_down)
    {
        Decision decision;
        if (terminated_) {
            if (!killed_ && now_ms - terminate_ms_ >= KILL_GRACE_MS) {
                killed_ = true;
                decision.send_kill = true;
            }
            return decision;
        }

        if (!esc_down) {
            if (hint_visible_) decision.hide_hint = true;
            pressed_ = false;
            hint_visible_ = false;
            return decision;
        }

        if (!pressed_) {
            pressed_ = true;
            press_ms_ = now_ms;
        }
        const std::uint64_t elapsed = now_ms - press_ms_;
        if (!hint_visible_ && elapsed >= HINT_DELAY_MS) {
            hint_visible_ = true;
            decision.show_hint = true;
        }
        if (elapsed >= TERMINATE_DELAY_MS) {
            terminated_ = true;
            terminate_ms_ = now_ms;
            decision.send_terminate = true;
        }
        return decision;
    }

    // poll(2) uses -1 when only input or child exit can change the state.
    int timeout_ms(std::uint64_t now_ms) const
    {
        if (killed_ || (!terminated_ && !pressed_)) return -1;
        const auto deadline = terminated_ ? terminate_ms_ + KILL_GRACE_MS :
            press_ms_ + (hint_visible_ ? TERMINATE_DELAY_MS : HINT_DELAY_MS);
        return now_ms >= deadline ? 0 : static_cast<int>(deadline - now_ms);
    }

    Decision finish()
    {
        Decision decision;
        decision.hide_hint = hint_visible_;
        hint_visible_ = false;
        return decision;
    }

private:
    std::uint64_t press_ms_ = 0;
    std::uint64_t terminate_ms_ = 0;
    bool pressed_ = false;
    bool hint_visible_ = false;
    bool terminated_ = false;
    bool killed_ = false;
};

} // namespace cp0_esc_exit_policy
