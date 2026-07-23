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
