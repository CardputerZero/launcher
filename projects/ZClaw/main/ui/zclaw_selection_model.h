#pragma once

namespace zclaw {

struct PagedSelection {
    int selected_index = 0;
    int scroll_offset = 0;
    int visible_row = 0;
};

int move_row_selection(int selected_row, int row_count, int delta);
PagedSelection move_paged_selection(int selected_index, int scroll_offset,
                                    int item_count, int visible_rows, int delta);

}  // namespace zclaw
