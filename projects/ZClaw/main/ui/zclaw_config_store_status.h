/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace zclaw {

enum class ConfigStoreLoadStatus {
    Loaded,
    NotFound,
    Invalid,
    Error,
};

}  // namespace zclaw
