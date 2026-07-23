#pragma once

typedef struct {
    int initialized;
    int thread_started;
    int deinitializing;
} cp0_keyboard_thread_lifecycle_t;

static inline int cp0_keyboard_thread_can_init(
    const cp0_keyboard_thread_lifecycle_t *state)
{
    return state && !state->initialized;
}

static inline void cp0_keyboard_thread_mark_started(
    cp0_keyboard_thread_lifecycle_t *state)
{
    if (!state) return;
    state->initialized = 1;
    state->thread_started = 1;
}

static inline int cp0_keyboard_thread_begin_deinit(
    cp0_keyboard_thread_lifecycle_t *state)
{
    if (!state || !state->initialized || !state->thread_started ||
        state->deinitializing)
        return 0;
    state->deinitializing = 1;
    return 1;
}

static inline void cp0_keyboard_thread_finish_deinit(
    cp0_keyboard_thread_lifecycle_t *state)
{
    if (!state) return;
    state->initialized = 0;
    state->thread_started = 0;
    state->deinitializing = 0;
}

static inline void cp0_keyboard_thread_cancel_deinit(
    cp0_keyboard_thread_lifecycle_t *state)
{
    if (state) state->deinitializing = 0;
}
