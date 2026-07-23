#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"
#include "../cp0_callback_contract.hpp"
#include "../cp0_soundcard_contract.hpp"
#include "../cp0_signal_registration.hpp"

#include <functional>
#include <list>
#include <memory>
#include <string>

namespace {

class SoundcardSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    SoundcardSystem()
    {
        operations_.unavailable_message = "not supported on SDL";
        operations_.unknown_command_message = "not supported on SDL";
    }

    void api_call(arg_t arg, callback_t callback)
    {
        const cp0::soundcard::Result result = cp0::soundcard::dispatch(arg, operations_);
        cp0::callback::invoke(callback, result.code, result.payload);
    }

private:
    cp0::soundcard::Operations operations_;
};

} // namespace

extern "C" void init_soundcard(void)
{
    static cp0::SignalRegistration<decltype(cp0_signal_soundcard_api)> registration;
    auto sc = std::make_shared<SoundcardSystem>();
    registration.replace(cp0_signal_soundcard_api,
        [sc](std::list<std::string> arg, std::function<void(int, std::string)> cb) {
            sc->api_call(std::move(arg), std::move(cb));
        });
}
