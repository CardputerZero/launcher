#include "zclaw_chat_layout.h"

#include <algorithm>

namespace zclaw {
namespace {

constexpr int kUserBubbleMaxWidth = 198;
constexpr int kUserBubbleMinWidth = 38;
constexpr int kUserBubblePadX = 10;
constexpr int kUserBubblePadY = 6;
constexpr int kTrackY = 20;
constexpr int kTrackHeight = 126;
constexpr int kMinimumThumbHeight = 18;

}  // namespace

ChatBubbleLayout user_bubble_layout(int text_width, int text_height)
{
    ChatBubbleLayout layout;
    layout.width = std::clamp(text_width + kUserBubblePadX * 2,
                              kUserBubbleMinWidth, kUserBubbleMaxWidth);
    layout.height = std::max(text_height + kUserBubblePadY * 2, 27);
    return layout;
}

int assistant_bubble_height(int text_height)
{
    return std::max(text_height + 12, 41);
}

int chat_scroll_delta(int scroll_top, int scroll_bottom, int requested_delta)
{
    const int top = std::max(scroll_top, 0);
    const int bottom = std::max(scroll_bottom, 0);
    if (requested_delta > 0)
        return std::min(requested_delta, top);
    if (requested_delta < 0)
        return std::max(requested_delta, -bottom);
    return 0;
}

ChatScrollbarLayout chat_scrollbar_layout(int scroll_top, int scroll_bottom)
{
    const int range = scroll_top + scroll_bottom;
    if (range <= 0)
        return {kTrackY, kTrackHeight};

    const int height = std::clamp(kTrackHeight * kTrackHeight /
                                      (kTrackHeight + range),
                                  kMinimumThumbHeight, kTrackHeight);
    const int travel = kTrackHeight - height;
    return {kTrackY + travel * scroll_top / range, height};
}

}  // namespace zclaw
