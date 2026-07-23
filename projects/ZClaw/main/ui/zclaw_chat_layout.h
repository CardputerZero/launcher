#pragma once

namespace zclaw {

struct ChatBubbleLayout {
    int width = 0;
    int height = 0;
};

struct ChatScrollbarLayout {
    int y = 0;
    int height = 0;
};

ChatBubbleLayout user_bubble_layout(int text_width, int text_height);
int assistant_bubble_height(int text_height);
int chat_scroll_delta(int scroll_top, int scroll_bottom, int requested_delta);
ChatScrollbarLayout chat_scrollbar_layout(int scroll_top, int scroll_bottom);

}  // namespace zclaw
