/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

/* Home-screen power menu opened by holding ESC.  Pure logic: no LVGL, so the
 * navigation and confirmation rules are unit-tested in test_power_menu_model.cpp. */

enum class PowerMenuItem : uint8_t
{
    Cancel,
    Lock,
    Reboot,
    Shutdown,
};

enum class PowerMenuStage : uint8_t
{
    Closed,
    Choosing,
    Confirming,
};

enum class PowerMenuKey : uint8_t
{
    Previous,
    Next,
    Enter,
    Back,
    Other,
};

struct PowerMenuDecision
{
    bool redraw = false;
    bool close = false;
    bool navigated = false;
    bool execute = false;
    PowerMenuItem action = PowerMenuItem::Cancel;
};

class PowerMenuModel
{
public:
    static constexpr size_t kItemCount = 4;

    static const char *label(PowerMenuItem item);
    static const char *confirm_title(PowerMenuItem item);
    static PowerMenuItem item_at(size_t index);

    void open();
    void close();
    PowerMenuDecision handle(PowerMenuKey key);

    PowerMenuStage stage() const { return stage_; }
    bool is_open() const { return stage_ != PowerMenuStage::Closed; }
    size_t selected_index() const { return selected_; }
    PowerMenuItem selected() const { return item_at(selected_); }

private:
    PowerMenuStage stage_ = PowerMenuStage::Closed;
    size_t selected_ = 0;
};
