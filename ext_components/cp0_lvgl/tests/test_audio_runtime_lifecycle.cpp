#include "cp0_audio_runtime_lifecycle.hpp"

#include <cassert>

int main()
{
    cp0::audio::RuntimeLifecycle lifecycle;
    assert(!lifecycle.active());
    assert(lifecycle.begin_init());
    assert(!lifecycle.begin_init());
    lifecycle.rollback_init();
    assert(!lifecycle.active());

    assert(lifecycle.begin_init());
    lifecycle.commit_init();
    assert(lifecycle.active());
    assert(!lifecycle.begin_init());
    assert(lifecycle.begin_shutdown());
    assert(!lifecycle.active());
    assert(!lifecycle.begin_shutdown());

    assert(lifecycle.begin_init());
    lifecycle.commit_init();
    assert(lifecycle.active());
}
