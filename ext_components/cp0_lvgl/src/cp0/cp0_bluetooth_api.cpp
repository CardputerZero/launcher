#include "cp0_bluetooth_backend.hpp"
#include "../cp0_init_once.hpp"
#include "../cp0_bluetooth_api_contract.hpp"

#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <functional>
#include <list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string encode_status(const cp0_bt_status_t &status)
{
    std::ostringstream output;
    output << status.powered << '\t'
           << cp0::bluetooth::sanitize_wire_field(status.address) << '\t'
           << status.discoverable << '\t'
           << cp0::bluetooth::sanitize_wire_field(status.alias);
    return output.str();
}

std::string encode_devices(const cp0_bt_device_t *devices, int count)
{
    std::ostringstream output;
    for(int i = 0; devices && i < count; ++i)
    {
        output << cp0::bluetooth::sanitize_wire_field(devices[i].address) << '\t'
               << devices[i].rssi << '\t'
               << devices[i].connected << '\t' << devices[i].paired << '\t'
               << devices[i].trusted << '\t'
               << cp0::bluetooth::sanitize_wire_field(devices[i].name) << '\n';
    }
    return output.str();
}

void report(const std::function<void(int, std::string)> &callback, int code, const std::string &data)
{
    cp0::bluetooth::invoke_callback(callback, code, data);
}

template <typename Loader>
cp0::bluetooth::Reply load_devices(int requested_count, Loader loader)
{
    std::vector<cp0_bt_device_t> devices(static_cast<size_t>(requested_count));
    int count = loader(devices.empty() ? nullptr : devices.data(), static_cast<int>(devices.size()));
    return {count, encode_devices(devices.data(), count)};
}

void api_call(std::list<std::string> args, std::function<void(int, std::string)> callback)
{
    using namespace cp0_bluetooth_backend;
    cp0::bluetooth::Request request;
    if (!cp0::bluetooth::parse_request(args, request)) {
        report(callback, -1, "invalid bt api request");
        return;
    }
    cp0::bluetooth::invoke_backend(callback, [&]() -> cp0::bluetooth::Reply {
        using cp0::bluetooth::Command;
        if(request.command == Command::Status) return {0, encode_status(status())};
        if(request.command == Command::Power) return {set_power(request.value), {}};
        if(request.command == Command::Alias) return {set_alias(request.text.c_str()), {}};
        if(request.command == Command::Discoverable) return {set_discoverable(request.value), {}};
        if(request.command == Command::Scan) return load_devices(request.max_count, scan);
        if(request.command == Command::DiscoveryStart) return {start_discovery(), {}};
        if(request.command == Command::DiscoveryStop) return {stop_discovery(), {}};
        if(request.command == Command::List)
            return load_devices(request.max_count, [](cp0_bt_device_t *out, int count) { return list(out, count, false); });
        if(request.command == Command::ConnectedList)
            return load_devices(request.max_count, [](cp0_bt_device_t *out, int count) { return list(out, count, true); });
        if(request.command == Command::Pair) return {pair(request.text.c_str()), {}};
        if(request.command == Command::Connect) return {connect(request.text.c_str()), {}};
        if(request.command == Command::Disconnect) return {disconnect(request.text.c_str()), {}};
        return {cp0_bluetooth_backend::remove(request.text.c_str()), {}};
    });
}

} // namespace

extern "C" void init_bluetooth(void)
{
    static cp0::InitOnce initialized;
    initialized.run([] {
        return static_cast<bool>(cp0_signal_bt_api.append(
            [](std::list<std::string> args,
               std::function<void(int, std::string)> callback) {
                api_call(std::move(args), std::move(callback));
            }));
    });
}
