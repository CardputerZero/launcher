#include "cp0_callback_result.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>

int main()
{
    int calls = 0;
    int code = 0;
    std::string data;
    cp0::CallbackResult result([&](int value, std::string text) {
        ++calls;
        code = value;
        data = std::move(text);
    });
    assert(!result.completed());
    assert(result.complete(7, "ready"));
    assert(result.completed());
    assert(calls == 1 && code == 7 && data == "ready");
    assert(!result.complete(-1, "duplicate"));
    assert(calls == 1 && code == 7 && data == "ready");

    cp0::CallbackResult throwing([](int, std::string) {
        throw std::runtime_error("callback failure");
    });
    assert(throwing.complete(0, "ok"));
    assert(throwing.completed());
    assert(!throwing.complete(-1, "retry"));

    cp0::CallbackResult empty({});
    assert(empty.complete(0, "ok"));
    assert(empty.completed());

    calls = 0;
    cp0::CallbackResult failed_operation([&](int value, std::string text) {
        ++calls;
        code = value;
        data = std::move(text);
    });
    failed_operation.guard(-1, "api failure", [] {
        throw std::runtime_error("backend failure");
    });
    assert(calls == 1 && code == -1 && data == "api failure");

    cp0::CallbackResult completed_operation([&](int, std::string) { ++calls; });
    completed_operation.guard(-1, "must not retry", [&] {
        completed_operation.complete(0, "ok");
        throw std::runtime_error("after completion");
    });
    assert(calls == 2);
}
