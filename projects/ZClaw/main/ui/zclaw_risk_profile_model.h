/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace zclaw {

struct RiskProfileUpdate {
    std::string contents;
    bool changed = false;
};

RiskProfileUpdate add_default_risk_profile(std::string config);

}  // namespace zclaw
