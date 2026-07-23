#include "cp0_alsa_parser.hpp"

#include <charconv>
#include <cstring>
#include <sstream>
#include <system_error>

namespace cp0_alsa_parser {
namespace {

std::string trim(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool parse_limits(const std::string &line, int &minimum, int &maximum)
{
    const size_t marker = line.find("Limits:");
    if (marker == std::string::npos) return false;
    std::string values = trim(line.substr(marker + 7));
    for (const char *prefix : {"Playback ", "Capture "}) {
        if (values.rfind(prefix, 0) == 0) {
            values = values.substr(std::strlen(prefix));
            break;
        }
    }
    const char *cursor = values.data();
    const char *end = cursor + values.size();
    auto first = std::from_chars(cursor, end, minimum);
    if (first.ec != std::errc{}) return false;
    cursor = first.ptr;
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    if (cursor == end || *cursor++ != '-') return false;
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    auto second = std::from_chars(cursor, end, maximum);
    if (second.ec != std::errc{}) return false;
    cursor = second.ptr;
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    return cursor == end;
}

bool is_value_line(const std::string &line)
{
    static constexpr const char *prefixes[] = {
        "Mono:", "Front Left:", "Front Right:", "Rear Left:", "Rear Right:",
        "Center:", "LFE:", "Side Left:", "Side Right:", "Capture:", "Playback:",
        "Item0:",
    };
    for (const char *prefix : prefixes) {
        if (line.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

bool parse_current_value(const std::string &line, int &value)
{
    const size_t separator = line.find(": ");
    if (separator == std::string::npos) return false;
    const char *cursor = line.c_str() + separator + 2;
    while (*cursor && *cursor != '-' && (*cursor < '0' || *cursor > '9')) ++cursor;
    if (!*cursor) return false;
    const char *end = line.data() + line.size();
    const auto parsed = std::from_chars(cursor, end, value);
    return parsed.ec == std::errc{};
}

struct Control {
    std::string name;
    std::string type;
    int minimum = 0;
    int maximum = 0;
    int step = 1;
    std::string current_text;
    int current_value = 0;
    bool active = false;
};

} // namespace

std::string encode_cards(const std::string &raw_cards)
{
    std::ostringstream output;
    std::istringstream lines(raw_cards);
    std::string line;
    while (std::getline(lines, line)) {
        const char *cursor = line.data();
        const char *end = cursor + line.size();
        while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
        int index = -1;
        const auto parsed = std::from_chars(cursor, end, index);
        if (parsed.ec != std::errc{} || index < 0) continue;
        cursor = parsed.ptr;
        while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
        if (cursor == end || *cursor != '[') continue;

        const size_t right_bracket = line.find(']');
        std::string display_name;
        if (right_bracket != std::string::npos) {
            const size_t colon = line.find(':', right_bracket);
            const size_t separator = colon == std::string::npos
                ? std::string::npos
                : line.find(" - ", colon);
            if (separator != std::string::npos) display_name = trim(line.substr(separator + 3));
        }
        if (display_name.empty()) {
            const size_t left_bracket = line.find('[');
            if (left_bracket != std::string::npos && right_bracket != std::string::npos &&
                right_bracket > left_bracket + 1) {
                display_name = trim(line.substr(left_bracket + 1,
                                                right_bracket - left_bracket - 1));
            }
        }
        output << index << "\tCard " << index;
        if (!display_name.empty()) output << ": " << display_name;
        output << '\n';
    }
    return output.str();
}

std::string encode_controls(const std::string &raw_controls)
{
    std::ostringstream output;
    std::istringstream lines(raw_controls);
    Control current;
    auto flush = [&]() {
        if (current.active && !current.name.empty()) {
            output << current.name << '\t' << current.type << '\t' << current.minimum << '\t'
                   << current.maximum << '\t' << current.step << '\t' << current.current_text
                   << '\t' << current.current_value << '\n';
        }
        current = Control{};
    };

    std::string line;
    while (std::getline(lines, line)) {
        const std::string text = trim(line);
        if (text.rfind("Simple mixer control", 0) == 0) {
            flush();
            const size_t first_quote = text.find('\'');
            const size_t second_quote = first_quote == std::string::npos
                ? std::string::npos
                : text.find('\'', first_quote + 1);
            if (second_quote != std::string::npos) {
                current.name = text.substr(first_quote + 1, second_quote - first_quote - 1);
            }
            current.active = true;
        } else if (current.active && text.rfind("Capabilities:", 0) == 0) {
            current.type = text.find("enum") != std::string::npos ? "ENUMERATED" : "INTEGER";
        } else if (current.active && text.rfind("Limits:", 0) == 0) {
            parse_limits(text, current.minimum, current.maximum);
        } else if (current.active && current.current_text.empty() && is_value_line(text)) {
            current.current_text = text;
            int value = 0;
            if (parse_current_value(text, value)) current.current_value = value;
        }
    }
    flush();
    return output.str();
}

} // namespace cp0_alsa_parser
