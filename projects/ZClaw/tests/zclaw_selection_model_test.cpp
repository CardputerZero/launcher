#include "zclaw_selection_model.h"

#include <cassert>

int main()
{
    assert(zclaw::move_row_selection(0, 5, -1) == 0);
    assert(zclaw::move_row_selection(2, 5, 1) == 3);
    assert(zclaw::move_row_selection(4, 5, 1) == 4);
    assert(zclaw::move_row_selection(4, 0, 1) == 0);

    zclaw::PagedSelection selection =
        zclaw::move_paged_selection(4, 0, 8, 5, 1);
    assert(selection.selected_index == 5);
    assert(selection.scroll_offset == 1);
    assert(selection.visible_row == 4);

    selection = zclaw::move_paged_selection(5, 1, 8, 5, -1);
    assert(selection.selected_index == 4);
    assert(selection.scroll_offset == 1);
    assert(selection.visible_row == 3);

    selection = zclaw::move_paged_selection(1, 1, 8, 5, -1);
    assert(selection.selected_index == 0);
    assert(selection.scroll_offset == 0);

    selection = zclaw::move_paged_selection(7, 99, 8, 5, 1);
    assert(selection.selected_index == 7);
    assert(selection.scroll_offset == 3);
    assert(selection.visible_row == 4);

    selection = zclaw::move_paged_selection(3, 2, 0, 5, 1);
    assert(selection.selected_index == 0);
    assert(selection.scroll_offset == 0);

    selection = zclaw::move_paged_selection(0, -3, 3, 0, 0);
    assert(selection.selected_index == 0);
    assert(selection.scroll_offset == 0);
    assert(selection.visible_row == 0);
    return 0;
}
