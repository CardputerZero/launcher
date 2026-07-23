#include "cp0_soundcard_contract.hpp"

#include "cp0_alsa_parser.hpp"

#include <charconv>
#include <system_error>

namespace cp0::soundcard {
namespace {

std::string argument_at(const std::list<std::string> &arguments, size_t index)
{
    auto iterator = arguments.begin();
    for (size_t current = 0; current < index && iterator != arguments.end(); ++current)
        ++iterator;
    return iterator == arguments.end() ? std::string() : *iterator;
}

bool parse_card_index(const std::string &text, int &index)
{
    if (text.empty())
        return false;
    const char *begin = text.data();
    const char *end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, index);
    return parsed.ec == std::errc{} && parsed.ptr == end && index >= 0;
}

Result unavailable(const Operations &operations)
{
    return {-1, operations.unavailable_message};
}

} // namespace

Result dispatch(const std::list<std::string> &arguments, const Operations &operations)
{
    try {
    const std::string command = argument_at(arguments, 0);
    if (command == "ListCards") {
        if (!operations.read_cards)
            return unavailable(operations);
        return {0, cp0_alsa_parser::encode_cards(operations.read_cards())};
    }

    int card_index = -1;
    if (command == "ListControls") {
        if (!parse_card_index(argument_at(arguments, 1), card_index))
            return {-1, {}};
        if (!operations.read_controls)
            return unavailable(operations);
        return {0, cp0_alsa_parser::encode_controls(operations.read_controls(card_index))};
    }
    if (command == "GetControlDetail") {
        const std::string control = argument_at(arguments, 2);
        if (!parse_card_index(argument_at(arguments, 1), card_index) || control.empty())
            return {-1, {}};
        if (!operations.read_control_detail)
            return unavailable(operations);
        return {0, operations.read_control_detail(card_index, control)};
    }
    if (command == "SetControl") {
        const std::string control = argument_at(arguments, 2);
        const std::string value = argument_at(arguments, 3);
        if (!parse_card_index(argument_at(arguments, 1), card_index) || control.empty() || value.empty())
            return {-1, {}};
        if (!operations.set_control)
            return unavailable(operations);
        const int code = operations.set_control(card_index, control, value);
        return {code, std::to_string(code)};
    }
        return {-1, operations.unknown_command_message};
    } catch (...) {
        return {-1, "soundcard backend failure"};
    }
}

} // namespace cp0::soundcard
