/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
/**
 * @file hal_sim.h
 * @brief Simulated HAL stubs for the M5CardputerZero emulator.
 *
 * These functions provide placeholder values for hardware peripherals
 * that don't exist on the desktop (battery gauge, RTC, etc.).
 */

#include <cstdint>
#include <ctime>

/**
 * @brief Returns the simulated battery percentage.
 * @return Always 100 (the emulator is "always plugged in").
 */
inline int sim_get_battery_percent()
{
    return 100;
}

/**
 * @brief Returns the current wall-clock time as if read from the on-board RTC.
 * @return The value of std::time(nullptr).
 */
inline std::time_t sim_get_rtc_time()
{
    return std::time(nullptr);
}
