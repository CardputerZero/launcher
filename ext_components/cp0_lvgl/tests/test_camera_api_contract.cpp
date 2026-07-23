#include "cp0_camera_api_contract.hpp"

#include <cassert>
#include <climits>

int main()
{
    using cp0_camera_api::parse_integer_argument;

    assert(cp0_camera_api::DEFAULT_WIDTH == 320);
    assert(cp0_camera_api::DEFAULT_HEIGHT == 150);
    assert(parse_integer_argument("42", 7) == 42);
    assert(parse_integer_argument("-3", 7) == -3);
    assert(parse_integer_argument("", 7) == 7);
    assert(parse_integer_argument("12px", 7) == 7);
    assert(parse_integer_argument(" 12", 7) == 12);
    assert(parse_integer_argument("\t+12", 7) == 12);
    assert(parse_integer_argument("12 ", 7) == 7);
    assert(parse_integer_argument("   ", 7) == 7);
    assert(parse_integer_argument("999999999999999999999", 7) == 7);
    assert(parse_integer_argument(std::to_string(INT_MAX), 7) == INT_MAX);
    assert(parse_integer_argument(std::to_string(INT_MIN), 7) == INT_MIN);
}
