/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <mutex>

namespace brightness_control {

inline std::mutex &operation_mutex()
{
    static std::mutex mutex;
    return mutex;
}

} // namespace brightness_control
