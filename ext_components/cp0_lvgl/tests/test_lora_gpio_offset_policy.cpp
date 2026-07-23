#include "cp0_lora_gpio_offset_policy.hpp"

#include <cassert>

int main()
{
    using namespace cp0_lora_gpio_offset_policy;

    auto result = resolve(nullptr, 5);
    assert(result.valid() && !result.overridden() && result.offset == 5);
    result = resolve("0", 5);
    assert(result.valid() && result.overridden() && result.offset == MIN_OFFSET);
    result = resolve("65535", 5);
    assert(result.valid() && result.overridden() && result.offset == MAX_OFFSET);

    for (const char *invalid : {"", "-1", "+1", " 5", "5 ", "5junk",
                                "65536", "2147483648", "999999999999999999999"})
        assert(!resolve(invalid, 5).valid());
    assert(!resolve(nullptr, -1).valid());
    assert(!resolve(nullptr, MAX_OFFSET + 1).valid());
}
