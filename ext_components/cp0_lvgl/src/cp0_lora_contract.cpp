#include "cp0_lora_contract.hpp"

#include <cstring>
#include <utility>

namespace cp0::lora {
namespace {

std::string argument_at(const std::list<std::string> &arguments, size_t index)
{
    auto iterator = arguments.begin();
    for (size_t current = 0; current < index && iterator != arguments.end(); ++current)
        ++iterator;
    return iterator == arguments.end() ? std::string() : *iterator;
}

bool valid_text_payload(const std::string &payload)
{
    return !payload.empty() && payload.size() <= kMaxTextPayloadBytes &&
           payload.find('\0') == std::string::npos;
}

} // namespace

std::string encode_info(const cp0_lora_info_t &info)
{
    return {reinterpret_cast<const char *>(&info), sizeof(info)};
}

bool decode_info(const std::string &payload, cp0_lora_info_t *info)
{
    if (!info || payload.size() != sizeof(*info))
        return false;
    std::memcpy(info, payload.data(), sizeof(*info));
    return true;
}

Result dispatch(const std::list<std::string> &arguments, const Operations &operations)
{
    try {
    if (arguments.empty())
        return {-1, "missing lora api command\n"};

    const std::string &command = arguments.front();
    if (command == "Init") {
        return operations.initialize && operations.initialize() ? Result{0, {}} : Result{-1, {}};
    }
    if (command == "Poll" || command == "Info") {
        if (!operations.read_info || (command == "Poll" && !operations.poll))
            return {-1, {}};
        if (command == "Poll")
            operations.poll();
        cp0_lora_info_t info{};
        operations.read_info(&info, command == "Poll");
        return {0, encode_info(info)};
    }
    if (command == "SendText") {
        const std::string payload = argument_at(arguments, 1);
        if (!valid_text_payload(payload) || !operations.send_text)
            return {-1, {}};
        return {operations.send_text(payload) ? 0 : -1, {}};
    }
    if (command == "StartReceive") {
        if (!operations.start_receive)
            return {-1, {}};
        operations.start_receive();
        return {0, {}};
    }
    if (command == "SetTxMode") {
        const std::string mode = argument_at(arguments, 1);
        if ((mode != "0" && mode != "1") || !operations.set_tx_mode)
            return {-1, {}};
        operations.set_tx_mode(mode == "1");
        return {0, {}};
    }
    if (command == "Shutdown") {
        if (!operations.shutdown)
            return {-1, {}};
        operations.shutdown();
        return {0, {}};
    }
        return {-1, "unknown lora api\n"};
    } catch (...) {
        return {-1, "lora backend failure\n"};
    }
}

Service::Service(Operations operations) : operations_(std::move(operations)) {}

Result Service::call(const std::list<std::string> &arguments)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return dispatch(arguments, operations_);
}

} // namespace cp0::lora
