/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "cp0_enum_cast.h"
#include "input_keys.h"

#include <cstdint>

/*
 * Lock-screen state machine layered on top of the screensaver.
 *
 *   Locked        (1) black screen
 *   PendingUnlock (2) visible screen waiting for the unlock gesture
 *   Armed         (3) gesture accepted, waiting for confirmation
 *
 * Every key belongs to the lock screen while it is up.  Only a fresh press
 * changes state: an auto-repeat is activity and nothing more, so holding a key
 * down cannot flap between the two visible states.  A release never changes
 * state either, which is what keeps the key that entered the lock (a held TAB)
 * from walking the machine forward on its own release.
 */
enum class LockscreenState {
    Locked,
    PendingUnlock,
    Armed,
};

struct LockscreenDecision
{
    bool consumed = false;
    /* Paint the visible screen: (1) -> (2)/(3) or (3) -> (2). */
    bool show_panel = false;
    /* The visible screen timed out; go back to the black (1) state. */
    bool sleep = false;
    /* Leave the lock screen entirely. */
    bool unlock = false;
    /* A fresh press that neither advanced nor confirmed the unlock, so the sound
     * feedback is "blocked" rather than "select". */
    bool blocked = false;
};

class LockscreenStateModel
{
public:
    enum class Metric : uint32_t {
        TimeoutMs = 10000,
    };

    static constexpr uint32_t timeout_ms()
    {
        return CP0_ENUM_CAST_UINT32(Metric::TimeoutMs);
    }

    LockscreenState state() const { return state_; }
    bool visible() const { return state_ != LockscreenState::Locked; }

    void reset(uint32_t now)
    {
        state_ = LockscreenState::Locked;
        last_activity_tick_ = now;
    }

    LockscreenDecision handle_key(uint32_t key_code, bool pressed, bool released,
                                  uint32_t now)
    {
        LockscreenDecision decision;
        decision.consumed = true;
        if (!released) last_activity_tick_ = now;
        if (!pressed) return decision;

        switch (state_) {
        case LockscreenState::Locked:
            state_ = LockscreenState::PendingUnlock;
            decision.show_panel = true;
            break;
        case LockscreenState::PendingUnlock:
            if (key_code == KEY_TAB) {
                state_ = LockscreenState::Armed;
                decision.show_panel = true;
            } else {
                decision.blocked = true;
            }
            break;
        case LockscreenState::Armed:
            if (key_code == KEY_ENTER || key_code == KEY_KPENTER) {
                decision.unlock = true;
            } else {
                /* Any other key steps back to (2) rather than unlocking. */
                state_ = LockscreenState::PendingUnlock;
                decision.show_panel = true;
                decision.blocked = true;
            }
            break;
        }
        return decision;
    }

    /* The visible states fall back to the black screen when nothing happens. */
    LockscreenDecision poll(uint32_t now)
    {
        LockscreenDecision decision;
        if (state_ == LockscreenState::Locked) return decision;
        if (now - last_activity_tick_ < timeout_ms()) return decision;
        state_ = LockscreenState::Locked;
        decision.consumed = true;
        decision.sleep = true;
        return decision;
    }

private:
    uint32_t last_activity_tick_ = 0;
    LockscreenState state_ = LockscreenState::Locked;
};
