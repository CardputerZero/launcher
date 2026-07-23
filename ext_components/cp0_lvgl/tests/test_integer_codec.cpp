#include "cp0_integer_codec.hpp"

#include <cassert>
#include <climits>
#include <cstdint>
#include <string>

int main()
{
    int value = 0;
    assert(cp0_integer_codec::parse_int("0", value) && value == 0);
    assert(cp0_integer_codec::parse_int("-1", value) && value == -1);
    assert(cp0_integer_codec::parse_int(std::to_string(INT_MAX), value) && value == INT_MAX);
    assert(cp0_integer_codec::parse_int(std::to_string(INT_MIN), value) && value == INT_MIN);
    assert(!cp0_integer_codec::parse_int("", value));
    assert(!cp0_integer_codec::parse_int("12junk", value));
    assert(!cp0_integer_codec::parse_int(" 12", value));
    assert(!cp0_integer_codec::parse_int("2147483648", value));
    assert(!cp0_integer_codec::parse_int("-2147483649", value));

    std::uint64_t unsigned_value = 17;
    assert(cp0_integer_codec::parse_decimal("0", std::uint64_t{0},
                                            UINT64_MAX, unsigned_value));
    assert(unsigned_value == 0);
    assert(cp0_integer_codec::parse_decimal("18446744073709551615", std::uint64_t{0},
                                            UINT64_MAX, unsigned_value));
    assert(unsigned_value == UINT64_MAX);
    for (const char *invalid : {"18446744073709551616", "+1", " 1", "1 "}) {
        unsigned_value = 17;
        assert(!cp0_integer_codec::parse_decimal(
            invalid, std::uint64_t{0}, UINT64_MAX, unsigned_value));
        assert(unsigned_value == 17);
    }
    assert(!cp0_integer_codec::parse_decimal(
        "5", std::uint64_t{6}, std::uint64_t{4}, unsigned_value));
    assert(unsigned_value == 17);

    value = 91;
    assert(cp0_integer_codec::parse_int_with_leading_whitespace(" \t+42", value));
    assert(value == 42);
    assert(cp0_integer_codec::parse_int_with_leading_whitespace("\n-3", value));
    assert(value == -3);
    assert(!cp0_integer_codec::parse_int_with_leading_whitespace("12tail", value));
    assert(value == -3);
    assert(!cp0_integer_codec::parse_int_with_leading_whitespace("   ", value));
    assert(value == -3);
    assert(!cp0_integer_codec::parse_int_with_leading_whitespace("2147483648", value));
    assert(value == -3);

    assert(cp0_integer_codec::parse_int_prefix_clamped("", 7) == 7);
    assert(cp0_integer_codec::parse_int_prefix_clamped("garbage", 7) == 0);
    assert(cp0_integer_codec::parse_int_prefix_clamped(" \t+12tail", 7) == 12);
    assert(cp0_integer_codec::parse_int_prefix_clamped("2147483648", 7) == INT_MAX);
    assert(cp0_integer_codec::parse_int_prefix_clamped("-2147483649", 7) == INT_MIN);

    std::uintmax_t identity = 0;
    assert(cp0_integer_codec::parse_non_root_identity("1000", UINT32_MAX, identity));
    assert(identity == 1000);
    assert(cp0_integer_codec::parse_non_root_identity("4294967294", UINT32_MAX, identity));
    assert(!cp0_integer_codec::parse_non_root_identity(nullptr, UINT32_MAX, identity));
    assert(!cp0_integer_codec::parse_non_root_identity("", UINT32_MAX, identity));
    assert(!cp0_integer_codec::parse_non_root_identity("0", UINT32_MAX, identity));
    assert(!cp0_integer_codec::parse_non_root_identity("-1", UINT32_MAX, identity));
    assert(!cp0_integer_codec::parse_non_root_identity("1000junk", UINT32_MAX, identity));
    assert(!cp0_integer_codec::parse_non_root_identity("4294967295", UINT32_MAX, identity));
    assert(!cp0_integer_codec::parse_non_root_identity("18446744073709551616",
                                                       UINT32_MAX, identity));
}
