#include "cp0_font_cache_policy.hpp"

#include <cassert>

int main()
{
    int font = 0;
    assert(cp0::font::should_cache(&font));
    assert(!cp0::font::should_cache(static_cast<int *>(nullptr)));
}
