/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_battery_info_model.hpp"

#include <array>
#include <charconv>
#include <climits>
#include <cstdio>
#include <string>

namespace {

bool parse_fields(const std::string &response, std::array<int, 12> &fields)
{
    std::size_t begin = 0;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const std::size_t end = index + 1 == fields.size()
                                    ? response.size()
                                    : response.find(',', begin);
        if (end == std::string::npos || end == begin) return false;

        const char *first = response.data() + begin;
        const char *last = response.data() + end;
        const auto parsed = std::from_chars(first, last, fields[index]);
        if (parsed.ec != std::errc{} || parsed.ptr != last) return false;
        begin = end + (index + 1 == fields.size() ? 0 : 1);
    }
    return begin == response.size();
}

bool valid_current(int value)
{
    return value == INT_MIN || (value >= -5000 && value <= 5000);
}

bool valid_average_current(int value)
{
    return value >= -5000 && value <= 5000;
}

bool valid_snapshot(const SettingsBatterySnapshot &snapshot)
{
    return snapshot.valid && snapshot.voltage_mv >= 0 && snapshot.voltage_mv <= 20000 &&
           valid_current(snapshot.current_ma) &&
           snapshot.temperature_c10 >= -400 && snapshot.temperature_c10 <= 1000 &&
           snapshot.soc >= 0 && snapshot.soc <= 100 && snapshot.remain_mah >= 0 &&
           snapshot.full_mah >= 0 &&
           (snapshot.full_mah == 0 || snapshot.remain_mah <= snapshot.full_mah) &&
           snapshot.flags >= 0 && valid_average_current(snapshot.avg_current_ma) &&
           snapshot.time_to_empty_min >= -1 && snapshot.time_to_full_min >= -1 &&
           snapshot.cycle_count >= -1;
}

std::string format_minutes(int minutes)
{
    char text[32];
    if (minutes >= 60)
        std::snprintf(text, sizeof(text), "%dh%02dm", minutes / 60, minutes % 60);
    else
        std::snprintf(text, sizeof(text), "%d min", minutes);
    return text;
}

} // namespace

bool SettingsBatteryInfoModel::parse_payload(const std::string &response,
                                             SettingsBatterySnapshot &snapshot)
{
    std::array<int, 12> fields{};
    if (!parse_fields(response, fields)) return false;

    SettingsBatterySnapshot parsed;
    parsed.voltage_mv = fields[0];
    parsed.current_ma = fields[1];
    parsed.temperature_c10 = fields[2];
    parsed.soc = fields[3];
    parsed.remain_mah = fields[4];
    parsed.full_mah = fields[5];
    parsed.flags = fields[6];
    parsed.avg_current_ma = fields[7];
    parsed.valid = fields[8] == 1;
    parsed.time_to_empty_min = fields[9];
    parsed.time_to_full_min = fields[10];
    parsed.cycle_count = fields[11];
    if (!valid_snapshot(parsed)) return false;

    snapshot = parsed;
    return true;
}

bool SettingsBatteryInfoModel::update(int result_code, const std::string &response)
{
    if (result_code != 0) {
        set_status("Battery read failed");
        return false;
    }

    SettingsBatterySnapshot parsed;
    if (!parse_payload(response, parsed)) {
        set_status("Invalid battery data");
        return false;
    }

    snapshot_ = parsed;
    state_ = SettingsBatteryReadState::Valid;
    status_text_ = "Battery updated";
    rebuild_labels();
    return true;
}

void SettingsBatteryInfoModel::set_status(const std::string &status)
{
    status_text_ = status.empty() ? "Battery unavailable" : status;
}

void SettingsBatteryInfoModel::invalidate(const std::string &reason)
{
    snapshot_ = {};
    state_ = SettingsBatteryReadState::Invalid;
    set_status(reason);
    rebuild_labels();
}

void SettingsBatteryInfoModel::rebuild_labels()
{
    if (!valid()) {
        labels_ = {
            "Battery: --%",
            "Temp: --C",
            "Current: --mA",
            "Voltage: --V",
            "Remaining: --mAh",
            "Full: --mAh",
            "Time: --",
            "Cycles: --",
        };
        values_ = {"--%", "-- C", "-- mA", "-- V", "-- mAh", "-- mAh", "--", "--"};
        return;
    }

    char text[64];
    std::snprintf(text, sizeof(text), "Battery: %d%%", snapshot_.soc);
    labels_[0] = text;
    std::snprintf(text, sizeof(text), "%d%%", snapshot_.soc);
    values_[0] = text;
    std::snprintf(text, sizeof(text), "Temp: %.1fC", snapshot_.temperature_c10 / 10.0);
    labels_[1] = text;
    std::snprintf(text, sizeof(text), "%.1f C", snapshot_.temperature_c10 / 10.0);
    values_[1] = text;
    if (snapshot_.current_ma == INT_MIN) {
        labels_[2] = "Current: --mA";
        values_[2] = "-- mA";
    } else {
        std::snprintf(text, sizeof(text), "Current: %dmA", snapshot_.current_ma);
        labels_[2] = text;
        std::snprintf(text, sizeof(text), "%d mA", snapshot_.current_ma);
        values_[2] = text;
    }
    std::snprintf(text, sizeof(text), "Voltage: %.2fV", snapshot_.voltage_mv / 1000.0);
    labels_[3] = text;
    std::snprintf(text, sizeof(text), "%.2f V", snapshot_.voltage_mv / 1000.0);
    values_[3] = text;
    if (snapshot_.full_mah > 0) {
        std::snprintf(text, sizeof(text), "Remaining: %dmAh", snapshot_.remain_mah);
        labels_[4] = text;
        std::snprintf(text, sizeof(text), "%d mAh", snapshot_.remain_mah);
        values_[4] = text;
        std::snprintf(text, sizeof(text), "Full: %dmAh", snapshot_.full_mah);
        labels_[5] = text;
        std::snprintf(text, sizeof(text), "%d mAh", snapshot_.full_mah);
        values_[5] = text;
    } else {
        labels_[4] = "Remaining: --mAh";
        values_[4] = "-- mAh";
        labels_[5] = "Full: --mAh";
        values_[5] = "-- mAh";
    }
    if (snapshot_.time_to_full_min >= 0) {
        values_[6] = format_minutes(snapshot_.time_to_full_min);
        labels_[6] = "Time to full: " + values_[6];
    } else if (snapshot_.time_to_empty_min >= 0) {
        values_[6] = format_minutes(snapshot_.time_to_empty_min);
        labels_[6] = "Time to empty: " + values_[6];
    } else {
        labels_[6] = "Time: --";
        values_[6] = "--";
    }
    if (snapshot_.cycle_count >= 0) {
        std::snprintf(text, sizeof(text), "Cycles: %d", snapshot_.cycle_count);
        labels_[7] = text;
        values_[7] = std::to_string(snapshot_.cycle_count);
    } else {
        labels_[7] = "Cycles: --";
        values_[7] = "--";
    }
}

std::string SettingsBatteryInfoModel::charge_state_text() const
{
    if (!valid()) return {};
    std::string text;
    if (snapshot_.flags & 1) text = "Charging";
    else if (snapshot_.flags & 2) text = "Full";
    else text = "Discharging";
    if (snapshot_.time_to_full_min >= 0)
        text += " - full in " + format_minutes(snapshot_.time_to_full_min);
    else if (snapshot_.time_to_empty_min >= 0)
        text += " - empty in " + format_minutes(snapshot_.time_to_empty_min);
    return text;
}
