/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <functional>
#include <string>
#include <utility>

namespace cp0::bluetooth::recovery {

using Completion = std::function<void(int, std::string)>;
using SetPower = std::function<void(bool, Completion)>;

// The entire sequence owns its callbacks independently of the page. Always
// attempt power-on, even if power-off timed out after reaching the adapter.
inline void restart(SetPower set_power, Completion complete)
{
    auto power_on = [set_power, complete](int off_code, std::string off_data) {
        auto finish = [complete, off_code, off_data](int code, std::string data) {
            if (code != 0 || (!data.empty() && data != "ok"))
                complete(-1, "Bluetooth power-on failed.");
            else if (off_code != 0 || (!off_data.empty() && off_data != "ok"))
                complete(-1, "Bluetooth power-off failed.");
            else
                complete(0, {});
        };
        try {
            set_power(true, finish);
        } catch (...) {
            finish(-1, {});
        }
    };
    try {
        set_power(false, power_on);
    } catch (...) {
        power_on(-1, {});
    }
}

} // namespace cp0::bluetooth::recovery
