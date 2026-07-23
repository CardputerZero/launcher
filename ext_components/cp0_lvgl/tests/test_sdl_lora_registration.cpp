#include "cp0_lora_contract.hpp"
#include "hal_lvgl_bsp.h"

#include <cassert>

eventpp::CallbackList<void(std::list<std::string>, std::function<void(int, std::string)>)>
    cp0_signal_lora_api;

extern "C" void init_lora(void);
extern "C" void deinit_lora(void) noexcept;

int main()
{
    static_assert(noexcept(deinit_lora()));
    init_lora();
    int callbacks = 0;
    cp0_signal_lora_api({"Init"}, [&](int code, std::string) {
        ++callbacks;
        assert(code == 0);
    });
    assert(callbacks == 1);
    cp0_signal_lora_api({"SetTxMode", "1"}, [](int code, std::string) {
        assert(code == 0);
    });

    init_lora();
    callbacks = 0;
    cp0_lora_info_t info{};
    cp0_signal_lora_api({"Info"}, [&](int code, std::string payload) {
        ++callbacks;
        assert(code == 0);
        assert(cp0::lora::decode_info(payload, &info));
    });
    assert(callbacks == 1);
    assert(info.initialized == 1);
    assert(info.tx_mode == 1);

    deinit_lora();
    callbacks = 0;
    cp0_signal_lora_api({"Info"}, [&](int, std::string) { ++callbacks; });
    assert(callbacks == 0);

    init_lora();
    cp0_signal_lora_api({"Info"}, [&](int code, std::string payload) {
        ++callbacks;
        assert(code == 0);
        assert(cp0::lora::decode_info(payload, &info));
    });
    assert(callbacks == 1);
    assert(info.initialized == 0);

    callbacks = 0;
    cp0_signal_lora_api({"Init"}, [&](int code, std::string) {
        ++callbacks;
        assert(code == 0);
    });
    assert(callbacks == 1);
    callbacks = 0;
    cp0_signal_lora_api({"Info"}, [&](int code, std::string payload) {
        ++callbacks;
        assert(code == 0);
        assert(cp0::lora::decode_info(payload, &info));
    });
    assert(callbacks == 1);
    assert(info.initialized == 1);
}
