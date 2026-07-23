#pragma once

typedef struct {
    void *handle;
    int creating;
} cp0_keyboard_input_lifecycle_t;

static inline int cp0_keyboard_input_begin_create(cp0_keyboard_input_lifecycle_t *state)
{
    if (!state || state->handle || state->creating) return 0;
    state->creating = 1;
    return 1;
}

static inline void cp0_keyboard_input_finish_create(cp0_keyboard_input_lifecycle_t *state,
                                                    void *handle)
{
    if (!state) return;
    state->handle = handle;
    state->creating = 0;
}

static inline int cp0_keyboard_input_handle_deleted(cp0_keyboard_input_lifecycle_t *state,
                                                    const void *handle)
{
    if (!state || !handle || state->handle != handle) return 0;
    state->handle = 0;
    state->creating = 0;
    return 1;
}
