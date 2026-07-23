#include "cp0_keyboard_input_lifecycle.h"

#include <assert.h>

int main(void)
{
    cp0_keyboard_input_lifecycle_t state = {0};
    int first = 1;
    int stale = 2;

    assert(cp0_keyboard_input_begin_create(&state));
    assert(!cp0_keyboard_input_begin_create(&state));
    cp0_keyboard_input_finish_create(&state, 0);
    assert(cp0_keyboard_input_begin_create(&state));
    cp0_keyboard_input_finish_create(&state, &first);
    assert(!cp0_keyboard_input_begin_create(&state));
    assert(!cp0_keyboard_input_handle_deleted(&state, &stale));
    assert(state.handle == &first);
    assert(cp0_keyboard_input_handle_deleted(&state, &first));
    assert(state.handle == 0);
    assert(cp0_keyboard_input_begin_create(&state));
}
