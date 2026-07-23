#include "../src/sdl/sdl_external_app_runner.hpp"

#include <cassert>
#include <chrono>

int main()
{
    assert(sdl_external_app_runner::run("exit 7", nullptr) == 7);

    volatile int exit_requested = 1;
    const auto start = std::chrono::steady_clock::now();
    assert(sdl_external_app_runner::run("trap '' TERM; while :; do sleep 1; done",
                                        &exit_requested) == -1);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    assert(elapsed >= std::chrono::seconds(5));
    assert(elapsed < std::chrono::seconds(8));
    assert(exit_requested == 0);
}
