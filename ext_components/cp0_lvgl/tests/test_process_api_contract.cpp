#include "cp0_process_api_contract.hpp"

#include <cassert>
#include <functional>
#include <cerrno>
#include <new>
#include <list>
#include <string>

struct ProcessOperation {
    int operator()() const { return 0; }
};

int main()
{
    static_assert(noexcept(cp0_process_api_contract::invoke_c_api(ProcessOperation{})));
    assert(cp0_process_api_contract::invoke_c_api([] { return 7; }) == 7);
    assert(cp0_process_api_contract::invoke_c_api(
        []() -> int { throw std::bad_alloc(); }) == -ENOMEM);
    assert(cp0_process_api_contract::invoke_c_api(
        []() -> int { throw 1; }) == -EIO);
    cp0_process_api_contract::invoke_c_api_void([] { throw 1; });
    const char *argv[] = {"apt-get", "install", "package with spaces", nullptr};
    assert(cp0_process_api_contract::make_run_sudo_request("secret", argv) ==
           (std::list<std::string>{"RunSudo", "secret", "apt-get", "install",
                                   "package with spaces"}));

    assert(cp0_process_api_contract::make_run_sudo_request(nullptr, nullptr) ==
           (std::list<std::string>{"RunSudo", ""}));

    using namespace cp0_process_api_contract;
    const std::list<std::string> request = {"Kill", "42", "1000"};
    assert(has_exact_arguments(request, 3));
    assert(has_at_least_arguments(request, 2));
    assert(argument_at(request, 1) && *argument_at(request, 1) == "42");
    assert(argument_at(request, 3) == nullptr);
    assert(arguments_from(request, 1) == (std::vector<std::string>{"42", "1000"}));

    bool flag = false;
    assert(parse_bool("1", flag) && flag);
    assert(parse_bool("0", flag) && !flag);
    assert(!parse_bool("2", flag));
    assert(!parse_bool("true", flag));

    int value = 0;
    assert(parse_pid("1", value) && value == 1);
    assert(!parse_pid("0", value));
    assert(!parse_pid("12junk", value));
    assert(!parse_pid(" 12", value));
    assert(parse_grace_ms("300000", value));
    assert(!parse_grace_ms("300001", value));
    assert(!parse_grace_ms("-1", value));
    assert(parse_delay_ms("0", value));
    assert(!parse_delay_ms("99999999999999999999", value));

    uintptr_t pointer = 0;
    assert(parse_pointer("0", pointer) && pointer == 0);
    assert(parse_pointer("123", pointer) && pointer == 123);
    assert(!parse_pointer("0x10", pointer));

    assert(parse_spawn_response("42", value) && value == 42);
    assert(!parse_spawn_response("0", value));
    assert(!parse_spawn_response("42junk", value));
    assert(parse_lock_holder_response("0", value) && value == 0);
    assert(!parse_lock_holder_response("-1", value));

    bool callback_called = false;
    invoke_callback_safely(
        std::function<void(int, std::string)>([&](int code, std::string data) {
            callback_called = code == 7 && data == "ok";
            throw 1;
        }),
        7, "ok");
    assert(callback_called);
    return 0;
}
