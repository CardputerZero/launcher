#include "cp0_callback_contract.hpp"

#include <cassert>
#include <functional>
#include <stdexcept>
#include <string>

namespace {

struct CopyThrows
{
    explicit CopyThrows(int &calls) : calls(calls) {}
    CopyThrows(const CopyThrows &other) : calls(other.calls)
    {
        throw std::runtime_error("copy");
    }
    void operator()() const { ++calls; }

    int &calls;
};

} // namespace

int main()
{
    int direct_calls = 0;
    cp0::callback::invoke_direct([&] { ++direct_calls; });
    assert(direct_calls == 1);

    CopyThrows no_copy(direct_calls);
    cp0::callback::invoke_direct(no_copy);
    assert(direct_calls == 2);

    cp0::callback::invoke_direct([] { throw std::runtime_error("delete callback"); });
    std::function<void()> empty;
    cp0::callback::invoke_direct(empty);

    int result_code = -1;
    std::string result_payload;
    cp0::callback::invoke([&](int code, std::string payload) {
        result_code = code;
        result_payload = std::move(payload);
    }, 7, "ready");
    assert(result_code == 7);
    assert(result_payload == "ready");

    cp0::callback::invoke([](int, std::string) {
        throw std::runtime_error("result callback");
    }, 0, "ignored");
}
