/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace zclaw {

bool ensure_private_parent_directory(const std::string &path,
                                     std::string *error = nullptr);

}  // namespace zclaw
