#pragma once

#include <functional>
#include <list>
#include <string>

namespace cp0::soundcard {

struct Result {
    int code = -1;
    std::string payload;
};

struct Operations {
    std::function<std::string()> read_cards;
    std::function<std::string(int)> read_controls;
    std::function<std::string(int, const std::string &)> read_control_detail;
    std::function<int(int, const std::string &, const std::string &)> set_control;
    std::string unavailable_message = "not supported";
    std::string unknown_command_message = "unknown soundcard api command";
};

Result dispatch(const std::list<std::string> &arguments, const Operations &operations);

} // namespace cp0::soundcard
