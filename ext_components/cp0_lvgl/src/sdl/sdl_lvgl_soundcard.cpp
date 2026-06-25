#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"

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

    void api_call(arg_t /*arg*/, callback_t callback)
    {
        if (callback) callback(-1, "not supported on SDL");
    }
};

} // namespace

extern "C" void init_soundcard(void)
{
    auto sc = std::make_shared<SoundcardSystem>();
    cp0_signal_soundcard_api.append(
        [sc](std::list<std::string> arg, std::function<void(int, std::string)> cb) {
            sc->api_call(std::move(arg), std::move(cb));
        });
}
