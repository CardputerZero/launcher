#include "cp0_soundcard_contract.hpp"
#include "cp0_callback_contract.hpp"

#include <cassert>
#include <climits>
#include <string>
#include <stdexcept>

int main()
{
    cp0::soundcard::Operations operations;
    int received_card = -1;
    std::string received_control;
    std::string received_value;
    operations.read_cards = [] {
        return std::string(" 0 [PCH ]: HDA-Intel - Built-in Audio\n");
    };
    operations.read_controls = [&](int card) {
        received_card = card;
        return std::string(
            "Simple mixer control 'Master',0\n"
            "  Capabilities: pvolume\n"
            "  Limits: Playback 0 - 100\n"
            "  Mono: Playback 50 [50%]\n");
    };
    operations.read_control_detail = [&](int card, const std::string &control) {
        received_card = card;
        received_control = control;
        return std::string("detail");
    };
    operations.set_control = [&](int card, const std::string &control, const std::string &value) {
        received_card = card;
        received_control = control;
        received_value = value;
        return 5;
    };

    auto result = cp0::soundcard::dispatch({"ListCards"}, operations);
    assert(result.code == 0 && result.payload == "0\tCard 0: Built-in Audio\n");
    result = cp0::soundcard::dispatch({"ListControls", "12"}, operations);
    assert(result.code == 0 && received_card == 12);
    assert(result.payload == "Master\tINTEGER\t0\t100\t1\tMono: Playback 50 [50%]\t50\n");
    result = cp0::soundcard::dispatch({"GetControlDetail", "2", "Master"}, operations);
    assert(result.code == 0 && result.payload == "detail");
    assert(received_card == 2 && received_control == "Master");
    result = cp0::soundcard::dispatch({"SetControl", "3", "Input Source", "Line"}, operations);
    assert(result.code == 5 && result.payload == "5");
    assert(received_card == 3 && received_control == "Input Source" && received_value == "Line");

    for (const std::string invalid : {"", "-1", "+1", "1junk", " 1", "2147483648",
                                      "999999999999999999999999"}) {
        assert(cp0::soundcard::dispatch({"ListControls", invalid}, operations).code == -1);
    }
    assert(cp0::soundcard::dispatch({"ListControls", std::to_string(INT_MAX)}, operations).code == 0);
    assert(received_card == INT_MAX);
    assert(cp0::soundcard::dispatch({"GetControlDetail", "0", ""}, operations).code == -1);
    assert(cp0::soundcard::dispatch({"SetControl", "0", "Master", ""}, operations).code == -1);
    assert(cp0::soundcard::dispatch({}, operations).payload == "unknown soundcard api command");
    assert(cp0::soundcard::dispatch({"Unknown"}, operations).code == -1);

    cp0::soundcard::Operations unavailable;
    unavailable.unavailable_message = "not supported on SDL";
    unavailable.unknown_command_message = "not supported on SDL";
    result = cp0::soundcard::dispatch({"ListCards"}, unavailable);
    assert(result.code == -1 && result.payload == "not supported on SDL");
    assert(cp0::soundcard::dispatch({"ListControls", "0"}, unavailable).payload ==
           "not supported on SDL");
    assert(cp0::soundcard::dispatch({"Unknown"}, unavailable).payload == "not supported on SDL");

    operations.read_cards = []() -> std::string { throw std::runtime_error("backend"); };
    result = cp0::soundcard::dispatch({"ListCards"}, operations);
    assert(result.code == -1 && result.payload == "soundcard backend failure");

    int callback_calls = 0;
    cp0::callback::invoke([&](int code, std::string payload) {
        ++callback_calls;
        assert(code == -1 && payload == "soundcard backend failure");
        throw std::runtime_error("callback");
    }, result.code, result.payload);
    assert(callback_calls == 1);
    cp0::callback::invoke({}, 0, "ignored");
}
