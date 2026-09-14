/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace cp0_desktop_exec_policy {

struct Result
{
    bool allowed;
    std::string reason;
};

Result evaluate(const char *exec);

} // namespace cp0_desktop_exec_policy
