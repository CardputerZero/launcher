#include "cp0_pointer_lifecycle.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    int first = 1;
    int second = 2;
    int *current = &first;

    assert(!cp0::can_mount_child(static_cast<int *>(nullptr),
                                 static_cast<int *>(nullptr)));
    assert(!cp0::can_mount_child(&first, &second));
    assert(cp0::can_mount_child(&first, static_cast<int *>(nullptr)));

    int *no_screen = nullptr;
    assert(cp0::display_references_screen(&first, &first, no_screen, no_screen));
    assert(cp0::display_references_screen(&first, no_screen, &first, no_screen));
    assert(cp0::display_references_screen(&first, no_screen, no_screen, &first));
    assert(!cp0::display_references_screen(&first, &second, no_screen, no_screen));
    assert(!cp0::display_references_screen(
        no_screen, no_screen, no_screen, no_screen));

    assert(!cp0::clear_if_deleted(current, static_cast<int *>(nullptr)));
    assert(!cp0::clear_if_deleted(current, &second));
    assert(current == &first);
    assert(cp0::clear_if_deleted(current, &first));
    assert(current == nullptr);

    current = &second;
    assert(!cp0::clear_if_deleted(current, &first));
    assert(current == &second);

    int *parent = &first;
    int *child = &second;
    assert(cp0::clear_if_deleted(child, &second));
    assert(parent == &first);
    assert(child == nullptr);
    assert(!cp0::clear_if_deleted(child, &second));
    assert(cp0::clear_if_deleted(parent, &first));
    assert(parent == nullptr);

    int *component_children[] = {&first, &second, &first};
    for (int *&owned : component_children)
        cp0::clear_if_deleted(owned, &first);
    assert(component_children[0] == nullptr);
    assert(component_children[1] == &second);
    assert(component_children[2] == nullptr);

    assert(cp0::is_direct_event_target(&first, &first));
    assert(!cp0::is_direct_event_target(&second, &first));
    assert(!cp0::is_direct_event_target(
        static_cast<int *>(nullptr), static_cast<int *>(nullptr)));
    static_assert(cp0::app_content_height(false) == 150);
    static_assert(cp0::app_content_height(true) == 130);
    assert(cp0::input_type_uses_group(1, 1, 2));
    assert(cp0::input_type_uses_group(2, 1, 2));
    assert(!cp0::input_type_uses_group(0, 1, 2));
    assert(!cp0::input_type_uses_group(3, 1, 2));

    int rollbacks = 0;
    {
        auto guard = cp0::make_rollback_guard([&] { ++rollbacks; });
    }
    assert(rollbacks == 1);
    {
        auto guard = cp0::make_rollback_guard([&] { ++rollbacks; });
        guard.dismiss();
    }
    assert(rollbacks == 1);
    {
        auto guard = cp0::make_rollback_guard([&] {
            ++rollbacks;
            throw std::runtime_error("rollback");
        });
    }
    assert(rollbacks == 2);
}
