#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <vector>

namespace cp0::pty {

enum class Command {
    open,
    read,
    write,
    resize,
    check_child,
    close,
};

struct Request {
    Command command = Command::open;
    std::uint64_t handle = 0;
    std::string executable;
    std::string data;
    int columns = 80;
    int rows = 24;
    std::size_t max_read = 4096;
    std::vector<std::string> argv;
};

struct ParseResult {
    bool ok = false;
    Request request;
    std::string error;
};

ParseResult parse_request(const std::list<std::string> &arguments);
std::string encode_handle(std::uint64_t handle);
bool decode_handle(const std::string &text, std::uint64_t &handle);

} // namespace cp0::pty
