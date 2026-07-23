#include "cp0_battery_api_contract.hpp"

#include "cp0_battery_codec.hpp"

#include <iterator>

namespace cp0::battery {
namespace {

constexpr int kCalibrationCommandCount = 4;

std::string argument_at(const std::list<std::string> &arguments, std::size_t index)
{
    if (index >= arguments.size()) return {};
    auto it = arguments.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(index));
    return *it;
}

bool parse_calibration_index(const std::string &text, int &index)
{
    if (text.size() != 1 || text.front() < '0' ||
        text.front() >= static_cast<char>('0' + kCalibrationCommandCount))
        return false;
    index = text.front() - '0';
    return true;
}

ApiParseResult failure(const std::string &error)
{
    ApiParseResult result;
    result.error = error;
    return result;
}

} // namespace

ApiParseResult parse_api_request(const std::list<std::string> &arguments)
{
    if (arguments.empty()) return failure("unknown bq27220 api command");

    ApiParseResult result;
    if (arguments.front() == "Read") {
        if (arguments.size() != 1) return failure("invalid bq27220 Read arguments");
        result.request.command = ApiCommand::read;
    } else if (arguments.front() == "Calibrate") {
        if (arguments.size() != 2 ||
            !parse_calibration_index(argument_at(arguments, 1), result.request.calibration_index))
            return failure("invalid bq27220 calibration command");
        result.request.command = ApiCommand::calibrate;
    } else {
        return failure("unknown bq27220 api command");
    }
    result.ok = true;
    return result;
}

ApiReply dispatch_api_request(const std::list<std::string> &arguments,
                              const ReadOperation &read,
                              const CalibrateOperation &calibrate)
{
    try {
    const ApiParseResult parsed = parse_api_request(arguments);
    if (!parsed.ok) return {-1, parsed.error};

    if (parsed.request.command == ApiCommand::read) {
        if (!read) return {-1, "battery reader unavailable"};
        const cp0_battery_info_t info = read();
        return {info.valid == 1 ? 0 : -1, encode_info(info)};
    }
    if (!calibrate) return {-1, "battery calibration unavailable"};
        return {calibrate(parsed.request.calibration_index), {}};
    } catch (...) {
        return {-1, "battery backend failure"};
    }
}

} // namespace cp0::battery
