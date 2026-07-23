#include "cp0_battery_api_contract.hpp"
#include "cp0_callback_contract.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    using cp0::battery::ApiCommand;
    using cp0::battery::dispatch_api_request;
    using cp0::battery::parse_api_request;

    auto read = parse_api_request({"Read"});
    assert(read.ok && read.request.command == ApiCommand::read);
    assert(!parse_api_request({"Read", "extra"}).ok);

    for (int index = 0; index < 4; ++index) {
        auto calibrate = parse_api_request({"Calibrate", std::to_string(index)});
        assert(calibrate.ok && calibrate.request.command == ApiCommand::calibrate);
        assert(calibrate.request.calibration_index == index);
    }
    assert(!parse_api_request({}).ok);
    assert(!parse_api_request({"Unknown"}).ok);
    assert(!parse_api_request({"Calibrate"}).ok);
    assert(!parse_api_request({"Calibrate", ""}).ok);
    assert(!parse_api_request({"Calibrate", "-1"}).ok);
    assert(!parse_api_request({"Calibrate", "+0"}).ok);
    assert(!parse_api_request({"Calibrate", " 0"}).ok);
    assert(!parse_api_request({"Calibrate", "4"}).ok);
    assert(!parse_api_request({"Calibrate", "0junk"}).ok);
    assert(!parse_api_request({"Calibrate", "999999999999999999999"}).ok);
    assert(!parse_api_request({"Calibrate", "0", "extra"}).ok);

    int read_calls = 0;
    int calibrate_calls = 0;
    auto reader = [&] {
        ++read_calls;
        return cp0_battery_info_t{4100, -125, 273, 81, 2430, 3000, 4, -120, 1};
    };
    auto calibrator = [&](int index) {
        ++calibrate_calls;
        return index == 2 ? -17 : 0;
    };

    auto read_reply = dispatch_api_request({"Read"}, reader, calibrator);
    assert(read_reply.code == 0);
    assert(read_reply.data == "4100,-125,273,81,2430,3000,4,-120,1");
    assert(read_calls == 1 && calibrate_calls == 0);

    auto calibrate_reply = dispatch_api_request({"Calibrate", "2"}, reader, calibrator);
    assert(calibrate_reply.code == -17 && calibrate_reply.data.empty());
    assert(read_calls == 1 && calibrate_calls == 1);

    auto invalid_reply = dispatch_api_request({"Calibrate", "2x"}, reader, calibrator);
    assert(invalid_reply.code == -1 && calibrate_calls == 1);

    auto invalid_read = dispatch_api_request(
        {"Read"}, [] { return cp0_battery_info_t{}; }, calibrator);
    assert(invalid_read.code == -1 && invalid_read.data == "0,0,0,0,0,0,0,0,0");
    assert(dispatch_api_request({"Read"}, {}, calibrator).code == -1);
    assert(dispatch_api_request({"Calibrate", "0"}, reader, {}).code == -1);

    auto throwing_read = dispatch_api_request(
        {"Read"}, []() -> cp0_battery_info_t { throw std::runtime_error("read"); }, calibrator);
    assert(throwing_read.code == -1 && throwing_read.data == "battery backend failure");
    auto throwing_calibrate = dispatch_api_request(
        {"Calibrate", "1"}, reader, [](int) -> int { throw std::runtime_error("calibrate"); });
    assert(throwing_calibrate.code == -1 && throwing_calibrate.data == "battery backend failure");

    int callback_calls = 0;
    cp0::callback::invoke([&](int code, std::string data) {
        ++callback_calls;
        assert(code == -1 && data == "battery backend failure");
        throw std::runtime_error("callback");
    }, throwing_read.code, throwing_read.data);
    assert(callback_calls == 1);
    return 0;
}
