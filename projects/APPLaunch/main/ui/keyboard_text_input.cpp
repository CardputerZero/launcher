#include "keyboard_text_input.hpp"

#include "input_keys.h"
#include "keyboard_input.h"

namespace launcher_ui::text_input {
namespace {

char fallback_character(uint32_t key)
{
    if (key >= KEY_1 && key <= KEY_9) return static_cast<char>('1' + key - KEY_1);
    if (key == KEY_0) return '0';
    static constexpr char qwerty[] = "qwertyuiop";
    if (key >= KEY_Q && key <= KEY_P) return qwerty[key - KEY_Q];
    static constexpr char asdf[] = "asdfghjkl";
    if (key >= KEY_A && key <= KEY_L) return asdf[key - KEY_A];
    static constexpr char zxcv[] = "zxcvbnm";
    if (key >= KEY_Z && key <= KEY_M) return zxcv[key - KEY_Z];
    if (key == KEY_SPACE) return ' ';
    if (key == KEY_DOT) return '.';
    if (key == KEY_MINUS) return '-';
    return 0;
}

} // namespace

std::string key_text(uint32_t key, const key_item *item)
{
    if (item && item->utf8[0] && static_cast<unsigned char>(item->utf8[0]) >= 0x20) {
        return item->utf8;
    }
    const char character = fallback_character(key);
    return character ? std::string(1, character) : std::string();
}

bool append_limited(std::string &buffer, const std::string &text, size_t max_bytes)
{
    if (text.empty() || buffer.size() + text.size() > max_bytes) return false;
    for (unsigned char character : text) {
        if (character < 0x20 || character == 0x7f) return false;
    }
    buffer += text;
    return true;
}

bool erase_last_codepoint(std::string &buffer)
{
    if (buffer.empty()) return false;
    size_t start = buffer.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(buffer[start]) & 0xc0) == 0x80) --start;
    buffer.erase(start);
    return true;
}

} // namespace launcher_ui::text_input
