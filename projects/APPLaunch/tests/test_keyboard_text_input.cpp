#include "../main/ui/keyboard_text_input.hpp"

#include "input_keys.h"
#include "keyboard_input.h"

#include <cassert>
#include <cstring>

int main()
{
    using launcher_ui::text_input::append_limited;
    using launcher_ui::text_input::erase_last_codepoint;
    using launcher_ui::text_input::key_text;

    assert(key_text(KEY_A, nullptr) == "a");
    assert(key_text(KEY_7, nullptr) == "7");
    assert(key_text(KEY_DOT, nullptr) == ".");
    assert(key_text(KEY_MINUS, nullptr) == "-");
    assert(key_text(KEY_ENTER, nullptr).empty());

    key_item item{};
    std::strcpy(item.utf8, "A");
    assert(key_text(KEY_A, &item) == "A");
    std::strcpy(item.utf8, "\xE7\x95\x8C");
    assert(key_text(0, &item) == "\xE7\x95\x8C");
    item.utf8[0] = '\n';
    item.utf8[1] = '\0';
    assert(key_text(0, &item).empty());

    std::string buffer = "abc";
    assert(append_limited(buffer, "de", 5) && buffer == "abcde");
    assert(!append_limited(buffer, "f", 5));
    assert(!append_limited(buffer, "\n", 8));
    assert(append_limited(buffer, "\xE7\x95\x8C", 8));
    assert(erase_last_codepoint(buffer) && buffer == "abcde");
    while (erase_last_codepoint(buffer)) {
    }
    assert(buffer.empty() && !erase_last_codepoint(buffer));
}
