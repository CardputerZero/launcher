/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace cp0::font {

template <typename Pointer>
bool should_cache(Pointer font)
{
    return font != nullptr;
}

} // namespace cp0::font
