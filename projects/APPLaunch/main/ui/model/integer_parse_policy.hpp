#pragma once

#include <charconv>
#include <string>
#include <system_error>
#include <type_traits>

namespace launcher_ui::integer_parse {

template <typename Integer>
bool complete(const std::string &text, Integer minimum, Integer maximum,
              Integer &output, int base = 10)
{
    static_assert(std::is_integral<Integer>::value, "Integer must be integral");
    if (text.empty()) return false;

    Integer parsed{};
    const char *begin = text.data();
    const char *end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed, base);
    if (result.ec != std::errc{} || result.ptr != end || parsed < minimum ||
        parsed > maximum)
        return false;

    output = parsed;
    return true;
}

} // namespace launcher_ui::integer_parse
