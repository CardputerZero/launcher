#pragma once

namespace cp0::font {

template <typename Pointer>
bool should_cache(Pointer font)
{
    return font != nullptr;
}

} // namespace cp0::font
