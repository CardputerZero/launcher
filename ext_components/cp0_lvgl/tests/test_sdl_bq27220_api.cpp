#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"

#include <cassert>
#include <stdexcept>

eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_bq27220_api;

extern "C" void init_bq27220(void);

int main()
{
    init_bq27220();
    init_bq27220();

    int callbacks = 0;
    int code = 0;
    cp0_signal_bq27220_api({}, [&](int result, std::string) {
        ++callbacks;
        code = result;
    });
    assert(callbacks == 1 && code == -1);

    const cp0_battery_info_t info = cp0_battery_read();
    assert(info.valid == 1 && info.soc >= 55 && info.soc <= 96);
    assert(cp0_bq27220_calibrate(0) == 0);
    assert(cp0_bq27220_calibrate(3) == 0);
    assert(cp0_bq27220_calibrate(-1) == -1);
    assert(cp0_bq27220_calibrate(4) == -1);

    cp0_signal_bq27220_api.append(
        [](std::list<std::string>, std::function<void(int, std::string)>) {
            throw std::runtime_error("signal");
        });
    assert(cp0_battery_read().valid == 0);
    assert(cp0_bq27220_calibrate(0) == -1);
    return 0;
}
