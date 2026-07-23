#pragma once

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace cp0_integer_codec {

namespace detail {

inline bool is_ascii_space(char character)
{
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

inline const char *skip_ascii_space(const char *begin, const char *end)
{
    while (begin < end && is_ascii_space(*begin)) ++begin;
    return begin;
}

} // namespace detail

template <typename Integer>
inline bool parse_decimal(std::string_view text,
                          Integer minimum,
                          Integer maximum,
                          Integer &value)
{
    if (text.empty() || minimum > maximum) return false;
    Integer parsed = 0;
    const char *begin = text.data();
    const char *end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr != end || parsed < minimum || parsed > maximum)
        return false;
    value = parsed;
    return true;
}

inline bool parse_int(const std::string &text, int &value)
{
    return parse_decimal(text, std::numeric_limits<int>::min(),
                         std::numeric_limits<int>::max(), value);
}

inline bool parse_int_with_leading_whitespace(const std::string &text, int &value)
{
    const char *end = text.data() + text.size();
    const char *begin = detail::skip_ascii_space(text.data(), end);
    if (begin == end) return false;
    bool positive_sign = *begin == '+';
    if (positive_sign && ++begin == end) return false;
    int parsed = 0;
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr != end) return false;
    value = parsed;
    return true;
}

inline int parse_int_prefix_clamped(const std::string &text, int fallback_if_empty)
{
    if (text.empty()) return fallback_if_empty;
    const char *end = text.data() + text.size();
    const char *begin = detail::skip_ascii_space(text.data(), end);
    bool negative = begin < end && *begin == '-';
    if (begin < end && *begin == '+') ++begin;
    int parsed = 0;
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ptr == begin) return 0;
    if (result.ec == std::errc::result_out_of_range)
        return negative ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    return parsed;
}

inline bool parse_non_root_identity(const char *text, std::uintmax_t invalid_sentinel,
                                    std::uintmax_t &value)
{
    if (!text || !text[0] || invalid_sentinel <= 1) return false;
    return parse_decimal(std::string_view(text), std::uintmax_t{1},
                         invalid_sentinel - 1, value);
}

} // namespace cp0_integer_codec
