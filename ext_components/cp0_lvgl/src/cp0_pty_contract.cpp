#include "cp0_pty_contract.hpp"

#include "cp0_integer_codec.hpp"

#include <algorithm>
#include <climits>
#include <iterator>
#include <limits>

namespace cp0::pty {
namespace {

constexpr int kDefaultColumns = 80;
constexpr int kDefaultRows = 24;
constexpr std::size_t kDefaultRead = 4096;
constexpr std::size_t kMaximumRead = 1024 * 1024;

std::string argument_at(const std::list<std::string> &arguments, std::size_t index)
{
    if (index >= arguments.size()) return {};
    auto it = arguments.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(index));
    return *it;
}

bool parse_positive_int(const std::string &text, int &value)
{
    return cp0_integer_codec::parse_decimal(text, 1, static_cast<int>(USHRT_MAX), value);
}

bool parse_read_size(const std::string &text, std::size_t &value)
{
    return cp0_integer_codec::parse_decimal(text, std::size_t{1}, kMaximumRead, value);
}

ParseResult failure(const std::string &message)
{
    ParseResult result;
    result.error = message;
    return result;
}

bool parse_required_handle(const std::list<std::string> &arguments, Request &request)
{
    return decode_handle(argument_at(arguments, 1), request.handle);
}

} // namespace

std::string encode_handle(std::uint64_t handle)
{
    return handle == 0 ? std::string() : std::to_string(handle);
}

bool decode_handle(const std::string &text, std::uint64_t &handle)
{
    handle = 0;
    return cp0_integer_codec::parse_decimal(
        text, std::uint64_t{1}, std::numeric_limits<std::uint64_t>::max(), handle);
}

ParseResult parse_request(const std::list<std::string> &arguments)
{
    if (arguments.empty()) return failure("empty pty api\n");

    ParseResult result;
    Request &request = result.request;
    const std::string command = arguments.front();
    if (command == "Open") {
        request.command = Command::open;
        request.executable = argument_at(arguments, 1);
        if (request.executable.empty()) return failure("empty pty command\n");
        if (!argument_at(arguments, 2).empty() &&
            !parse_positive_int(argument_at(arguments, 2), request.columns))
            return failure("invalid pty columns\n");
        if (!argument_at(arguments, 3).empty() &&
            !parse_positive_int(argument_at(arguments, 3), request.rows))
            return failure("invalid pty rows\n");
        if (argument_at(arguments, 2).empty()) request.columns = kDefaultColumns;
        if (argument_at(arguments, 3).empty()) request.rows = kDefaultRows;
        auto it = arguments.begin();
        std::advance(it, std::min<std::size_t>(4, arguments.size()));
        request.argv.assign(it, arguments.end());
    } else if (command == "Read") {
        request.command = Command::read;
        if (!parse_required_handle(arguments, request)) return failure("invalid pty handle\n");
        request.max_read = kDefaultRead;
        if (!argument_at(arguments, 2).empty() &&
            !parse_read_size(argument_at(arguments, 2), request.max_read))
            return failure("invalid pty read size\n");
    } else if (command == "Write") {
        request.command = Command::write;
        if (!parse_required_handle(arguments, request)) return failure("invalid pty handle\n");
        if (arguments.size() < 3) return failure("missing pty write data\n");
        request.data = argument_at(arguments, 2);
    } else if (command == "Resize") {
        request.command = Command::resize;
        if (!parse_required_handle(arguments, request)) return failure("invalid pty handle\n");
        if (!parse_positive_int(argument_at(arguments, 2), request.columns) ||
            !parse_positive_int(argument_at(arguments, 3), request.rows))
            return failure("invalid pty size\n");
    } else if (command == "CheckChild") {
        request.command = Command::check_child;
        if (!parse_required_handle(arguments, request)) return failure("invalid pty handle\n");
    } else if (command == "Close") {
        request.command = Command::close;
        if (!parse_required_handle(arguments, request)) return failure("invalid pty handle\n");
    } else {
        return failure("unknown pty api: " + command + "\n");
    }

    result.ok = true;
    return result;
}

} // namespace cp0::pty
