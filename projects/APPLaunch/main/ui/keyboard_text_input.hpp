#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct key_item;

namespace launcher_ui::text_input {

std::string key_text(uint32_t key, const key_item *item);
bool append_limited(std::string &buffer, const std::string &text, size_t max_bytes);
bool erase_last_codepoint(std::string &buffer);

} // namespace launcher_ui::text_input
