/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "cp0_enum_cast.h"

#include <cstddef>
#include <cstdint>

struct ScreensaverFrame
{
    int x = 0;
    int y = 0;
    size_t color_index = 0;
    bool color_changed = false;
};

struct ScreensaverHoldDecision
{
    /* The hold matured past the hint delay: announce what the gesture will do. */
    bool show_hint = false;
    /* A visible hint was cleared (release, another key, or a state change), so
     * the caller must take the announcement down again. */
    bool hide_hint = false;
    /* The hold matured past the lock delay: enter the lock screen. */
    bool fire = false;
};

class ScreensaverModel
{
public:
    enum class BlockMetric : int {
        Size = 50,
    };
    enum class ColorMetric : std::size_t {
        Count = 8,
    };
    enum class ScreenOffMetric : uint32_t {
        HoldMs = 3000,
    };
    enum class HoldHintMetric : uint32_t {
        HintMs = 500,
    };

    static constexpr int block_size()
    {
        return CP0_ENUM_CAST_INT(BlockMetric::Size);
    }

    static constexpr std::size_t color_count()
    {
        return CP0_ENUM_CAST_SIZE_T(ColorMetric::Count);
    }

    static constexpr uint32_t screen_off_hold_ms()
    {
        return CP0_ENUM_CAST_UINT32(ScreenOffMetric::HoldMs);
    }

    static constexpr uint32_t hold_hint_ms()
    {
        return CP0_ENUM_CAST_UINT32(HoldHintMetric::HintMs);
    }

    void reset(uint32_t now);
    void set_foreground(bool foreground, uint32_t now);
    bool should_activate(uint32_t now, uint32_t timeout_ms, bool runtime_ready) const;
    ScreensaverFrame activate(int width, int height, uint32_t now);
    void deactivate();
    ScreensaverFrame advance(int width, int height, uint32_t now);

    /* Keys belong to the lock-screen state machine once the screensaver is up.
     * While it is idle the screensaver only needs to know that something
     * happened, so the next automatic activation is measured from here. */
    void note_activity(uint32_t now);

    /* Observation-only tracking of the long-press that requests the lock.
     * It never reports a consume decision: the press and release must keep
     * reaching the page underneath, which owns the short-tap meaning of the
     * same key.  poll_hold() reports the hint maturing and the gesture firing. */
    ScreensaverHoldDecision observe_hold_key(uint32_t key_code, bool released, uint32_t now);
    ScreensaverHoldDecision poll_hold(uint32_t now);

    bool active() const { return active_; }
    bool hold_pending() const { return hold_pending_; }
    bool hold_hint_visible() const { return hold_hint_shown_; }
    bool foreground() const { return foreground_; }
    uint32_t last_activity_tick() const { return last_activity_tick_; }

private:
    ScreensaverFrame frame(bool color_changed = false) const;
    void clear_hold();

    uint32_t last_activity_tick_ = 0;
    uint32_t last_frame_tick_ = 0;
    uint32_t hold_down_tick_ = 0;
    int32_t x_milli_ = 0;
    int32_t y_milli_ = 0;
    int velocity_x_ = 45;
    int velocity_y_ = 35;
    size_t color_index_ = 0;
    bool active_ = false;
    bool foreground_ = true;
    bool hold_pending_ = false;
    bool hold_fired_ = false;
    bool hold_hint_shown_ = false;
};
