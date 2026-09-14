/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <utility>

namespace cp0::signal {

template <typename Operation>
bool invoke_noexcept(Operation &&operation) noexcept
{
    try {
        std::forward<Operation>(operation)();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace cp0::signal
