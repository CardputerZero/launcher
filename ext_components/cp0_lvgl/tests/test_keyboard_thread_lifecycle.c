#include "cp0_keyboard_thread_lifecycle.h"

#include <assert.h>

int main(void)
{
    cp0_keyboard_thread_lifecycle_t state = {0};

    assert(cp0_keyboard_thread_can_init(&state));
    cp0_keyboard_thread_mark_started(&state);
    assert(!cp0_keyboard_thread_can_init(&state));

    /* A naturally exited pthread remains joinable until deinit reaps it. */
    assert(cp0_keyboard_thread_begin_deinit(&state));
    assert(!cp0_keyboard_thread_can_init(&state));
    assert(!cp0_keyboard_thread_begin_deinit(&state));
    cp0_keyboard_thread_finish_deinit(&state);

    assert(cp0_keyboard_thread_can_init(&state));
    cp0_keyboard_thread_mark_started(&state);
    assert(cp0_keyboard_thread_begin_deinit(&state));
    cp0_keyboard_thread_cancel_deinit(&state);
    assert(!cp0_keyboard_thread_can_init(&state));
    assert(cp0_keyboard_thread_begin_deinit(&state));
    cp0_keyboard_thread_finish_deinit(&state);
    assert(cp0_keyboard_thread_can_init(&state));
}
