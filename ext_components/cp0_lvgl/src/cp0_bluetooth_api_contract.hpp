#pragma once

#include <functional>
#include <list>
#include <string>

namespace cp0::bluetooth {

enum class Command {
    Status, Power, Alias, Discoverable, Scan, DiscoveryStart, DiscoveryStop,
    List, ConnectedList, Pair, Connect, Disconnect, Remove,
};

struct Request {
    Command command = Command::Status;
    int value = 0;
    int max_count = 16;
    std::string text;
};

struct Reply {
    int code = -1;
    std::string data;
};

bool parse_request(const std::list<std::string> &arguments, Request &request);
std::string sanitize_wire_field(std::string value);
void invoke_callback(const std::function<void(int, std::string)> &callback,
                     int code, const std::string &data) noexcept;

template <typename BackendOperation>
void invoke_backend(const std::function<void(int, std::string)> &callback,
                    BackendOperation operation) noexcept
{
    Reply reply;
    try {
        reply = operation();
    } catch (...) {
        reply = {-1, "bluetooth backend failure"};
    }
    invoke_callback(callback, reply.code, reply.data);
}

} // namespace cp0::bluetooth
