/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <string>

struct SettingsBatterySnapshot {
    int voltage_mv = 0;
    int current_ma = 0;
    int temperature_c10 = 0;
    int soc = 0;
    int remain_mah = 0;
    int full_mah = 0;
    int flags = 0;
    int avg_current_ma = 0;
    bool valid = false;
    int time_to_empty_min = -1;
    int time_to_full_min = -1;
    int cycle_count = -1;
};

enum class SettingsBatteryReadState {
    Invalid,
    Valid,
};

class SettingsBatteryInfoModel {
public:
    enum class ValueMetric : std::size_t {
        Count = 8,
    };

    enum class LabelMetric : std::size_t {
        Count = static_cast<std::size_t>(ValueMetric::Count),
    };

    bool update(int result_code, const std::string &response);
    void set_status(const std::string &status);
    void invalidate(const std::string &reason = "Battery unavailable");

    const SettingsBatterySnapshot &snapshot() const { return snapshot_; }
    const std::array<std::string, static_cast<std::size_t>(LabelMetric::Count)> &labels() const
    {
        return labels_;
    }
    const std::array<std::string, static_cast<std::size_t>(ValueMetric::Count)> &values() const
    {
        return values_;
    }
    SettingsBatteryReadState state() const { return state_; }
    bool valid() const { return state_ == SettingsBatteryReadState::Valid; }
    const std::string &status_text() const { return status_text_; }
    std::string charge_state_text() const;

    static bool parse_payload(const std::string &response,
                              SettingsBatterySnapshot &snapshot);

private:
    void rebuild_labels();

    SettingsBatterySnapshot snapshot_;
    SettingsBatteryReadState state_ = SettingsBatteryReadState::Invalid;
    std::string status_text_ = "Battery unavailable";
    std::array<std::string, static_cast<std::size_t>(LabelMetric::Count)> labels_ = {
        "Battery: --%",
        "Temp: --C",
        "Current: --mA",
        "Voltage: --V",
        "Remaining: --mAh",
        "Full: --mAh",
        "Time: --",
        "Cycles: --",
    };
    std::array<std::string, static_cast<std::size_t>(ValueMetric::Count)> values_ = {
        "--%", "-- C", "-- mA", "-- V", "-- mAh", "-- mAh", "--", "--",
    };
};
