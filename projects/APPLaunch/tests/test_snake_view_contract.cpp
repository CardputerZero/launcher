#include "../main/ui/model/snake_view_contract.hpp"

#include <cassert>

int main()
{
    int target = 1;
    int bubbled_target = 2;
    assert(snake_owned_delete_callback_allowed(&target, &target));
    assert(!snake_owned_delete_callback_allowed(&target, &bubbled_target));
    assert(!snake_owned_delete_callback_allowed(
        static_cast<int *>(nullptr), &target));

    int game_area = 3;
    int render_layer = 4;
    static_assert(noexcept(snake_tick_callback_allowed(
        false, false, false, static_cast<int *>(nullptr),
        static_cast<int *>(nullptr))));
    assert(snake_tick_callback_allowed(
        true, true, true, &game_area, &render_layer));
    assert(!snake_tick_callback_allowed(
        false, true, true, &game_area, &render_layer));
    assert(!snake_tick_callback_allowed(
        true, false, true, &game_area, &render_layer));
    assert(!snake_tick_callback_allowed(
        true, true, false, &game_area, &render_layer));
    assert(!snake_tick_callback_allowed(
        true, true, true, &game_area, static_cast<int *>(nullptr)));

    int root_screen = 5;
    int stale_root = 6;
    static_assert(noexcept(snake_page_event_callback_allowed(
        static_cast<int *>(nullptr), static_cast<int *>(nullptr))));
    assert(snake_page_event_callback_allowed(&root_screen, &root_screen));
    assert(!snake_page_event_callback_allowed(&stale_root, &root_screen));
    assert(!snake_page_event_callback_allowed(
        static_cast<int *>(nullptr), &root_screen));

    SnakeFrameBuildContract empty(0);
    assert(empty.ready());

    SnakeFrameBuildContract frame(4);
    assert(!frame.ready());
    for (int cell = 0; cell < 3; ++cell) frame.cell_created();
    assert(!frame.ready());
    frame.cell_created();
    assert(frame.ready());
    frame.cell_created();
    assert(!frame.ready());
}
