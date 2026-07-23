#include "zclaw_selection_model.h"

namespace zclaw {

int move_row_selection(int selected_row, int row_count, int delta)
{
    if (row_count <= 0)
        return 0;
    const int moved = selected_row + delta;
    if (moved < 0)
        return 0;
    if (moved >= row_count)
        return row_count - 1;
    return moved;
}

PagedSelection move_paged_selection(int selected_index, int scroll_offset,
                                    int item_count, int visible_rows, int delta)
{
    if (item_count <= 0)
        return {};
    if (visible_rows <= 0)
        visible_rows = 1;

    selected_index = move_row_selection(selected_index, item_count, delta);
    const int maximum_scroll = item_count > visible_rows ? item_count - visible_rows : 0;
    if (scroll_offset < 0)
        scroll_offset = 0;
    if (scroll_offset > maximum_scroll)
        scroll_offset = maximum_scroll;
    if (selected_index < scroll_offset)
        scroll_offset = selected_index;
    if (selected_index >= scroll_offset + visible_rows)
        scroll_offset = selected_index - visible_rows + 1;
    return {selected_index, scroll_offset, selected_index - scroll_offset};
}

}  // namespace zclaw
