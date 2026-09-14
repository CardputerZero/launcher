/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace zclaw {

class RiskProfileStore {
public:
    bool ensure_default(const std::string &path, std::string *error) const;
};

}  // namespace zclaw
