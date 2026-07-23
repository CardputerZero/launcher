#pragma once

#include <charconv>
#include <string>

namespace cp0_lora_gpio_offset_policy {

constexpr int MIN_OFFSET = 0;
constexpr int MAX_OFFSET = 65535;

enum class Source
{
    DEFAULT_VALUE,
    ENVIRONMENT,
    INVALID,
};

struct Resolution
{
    Source source = Source::INVALID;
    int offset = 0;

    bool valid() const { return source != Source::INVALID; }
    bool overridden() const { return source == Source::ENVIRONMENT; }
};

inline Resolution resolve(const char *environment_value, int default_offset)
{
    auto in_range = [](int value) {
        return value >= MIN_OFFSET && value <= MAX_OFFSET;
    };
    if (!environment_value) {
        return in_range(default_offset)
            ? Resolution{Source::DEFAULT_VALUE, default_offset} : Resolution{};
    }
    if (!environment_value[0]) return {};

    const std::string text(environment_value);
    int parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        !in_range(parsed))
        return {};
    return {Source::ENVIRONMENT, parsed};
}

} // namespace cp0_lora_gpio_offset_policy
