/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace launcher_battery_ui {

enum class LowBatteryWarning {
    None,
    Undefined,
    Reminder,
    Low,
    ShutdownCountdown,
};

class LowBatteryFlow {
public:
    static constexpr uint32_t kShutdownDelayMs = 15000;

    void reset() { *this = LowBatteryFlow{}; }

    void update(bool valid, int soc, bool charging, uint32_t now)
    {
        if (!valid) {
            warning_ = LowBatteryWarning::Undefined;
            shutdown_requested_ = false;
            return;
        }
        if (charging || soc > 20 || soc == 5) {
            reminder_shown_ = false;
            warning_ = LowBatteryWarning::None;
            shutdown_requested_ = false;
            return;
        }
        if (soc > 5) {
            shutdown_requested_ = false;
            reminder_soc_ = soc;
            if (!reminder_shown_) {
                reminder_shown_ = true;
                warning_ = LowBatteryWarning::Reminder;
            } else if (warning_ != LowBatteryWarning::Reminder) {
                warning_ = LowBatteryWarning::None;
            }
            return;
        }
        reminder_shown_ = false;
        if (soc <= 3) {
            if (warning_ != LowBatteryWarning::ShutdownCountdown) {
                countdown_started_ = now;
                shutdown_requested_ = false;
            }
            warning_ = LowBatteryWarning::ShutdownCountdown;
            return;
        }
        warning_ = LowBatteryWarning::Low;
        shutdown_requested_ = false;
    }

    void dismiss_reminder()
    {
        if (warning_ == LowBatteryWarning::Reminder)
            warning_ = LowBatteryWarning::None;
    }

    int reminder_soc() const { return reminder_soc_; }

    bool take_shutdown_due(uint32_t now)
    {
        if (!shutdown_due(now))
            return false;
        shutdown_requested_ = true;
        return true;
    }

    bool shutdown_due(uint32_t now) const
    {
        return warning_ == LowBatteryWarning::ShutdownCountdown && !shutdown_requested_ &&
            static_cast<uint32_t>(now - countdown_started_) >= kShutdownDelayMs;
    }

    LowBatteryWarning warning() const { return warning_; }

    uint32_t seconds_until_shutdown(uint32_t now) const
    {
        if (warning_ != LowBatteryWarning::ShutdownCountdown)
            return 0;
        const uint32_t elapsed = static_cast<uint32_t>(now - countdown_started_);
        if (elapsed >= kShutdownDelayMs)
            return 0;
        return (kShutdownDelayMs - elapsed + 999) / 1000;
    }

private:
    LowBatteryWarning warning_ = LowBatteryWarning::None;
    uint32_t countdown_started_ = 0;
    bool shutdown_requested_ = false;
    // Keep this latch through invalid reads to avoid repeating a reminder on recovery.
    bool reminder_shown_ = false;
    int reminder_soc_ = 0;
};

} // namespace launcher_battery_ui
