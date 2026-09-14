/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace zclaw {

enum class TextFileReadStatus {
    Loaded,
    NotFound,
    Error,
};

struct TextFileReadResult {
    TextFileReadStatus status = TextFileReadStatus::Error;
    std::string contents;
    std::string error;
};

TextFileReadResult read_text_file(const std::string &path);

}  // namespace zclaw
