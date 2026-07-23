#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"
#include "../cp0_callback_contract.hpp"
#include "../cp0_soundcard_contract.hpp"
#include "../cp0_signal_registration.hpp"

#include <cstdio>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace {

class SoundcardSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    SoundcardSystem()
    {
        operations_.read_cards = [] { return capture_argv({"cat", "/proc/asound/cards"}); };
        operations_.read_controls = [](int card_index) {
            const std::string card = std::to_string(card_index);
            return capture_argv({"amixer", "-c", card.c_str()});
        };
        operations_.read_control_detail = [](int card_index, const std::string &control) {
            const std::string card = std::to_string(card_index);
            return capture_argv({"amixer", "-c", card.c_str(), "sget", control.c_str()});
        };
        operations_.set_control = [](int card_index, const std::string &control,
                                     const std::string &value) {
            const std::string card = std::to_string(card_index);
            return run_argv({"amixer", "-c", card.c_str(), "sset", control.c_str(), value.c_str()});
        };
    }

    void api_call(arg_t arg, callback_t callback)
    {
        const cp0::soundcard::Result result = cp0::soundcard::dispatch(arg, operations_);
        cp0::callback::invoke(callback, result.code, result.payload);
    }

private:
    cp0::soundcard::Operations operations_;

    static std::string capture_argv(std::initializer_list<const char *> args)
    {
        std::list<std::string> req;
        req.push_back("CaptureArgv");
        for (const char *a : args)
            if (a) req.push_back(a);

        std::string out;
        cp0_signal_process_api(req, [&](int, std::string data) {
            out = std::move(data);
        });
        return out;
    }

    static int run_argv(std::initializer_list<const char *> args)
    {
        std::list<std::string> req;
        req.push_back("RunArgv");
        req.push_back("0");
        for (const char *a : args)
            if (a) req.push_back(a);

        int rc = -1;
        cp0_signal_process_api(req, [&](int code, std::string) {
            rc = code;
        });
        return rc;
    }

};

} // namespace

extern "C" void init_soundcard(void)
{
    static cp0::SignalRegistration<decltype(cp0_signal_soundcard_api)> registration;
    auto soundcard = std::make_shared<SoundcardSystem>();
    registration.replace(cp0_signal_soundcard_api, [soundcard](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        soundcard->api_call(std::move(arg), std::move(callback));
    });
}
