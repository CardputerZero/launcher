#include "cp0_camera_api_contract.hpp"
#include "cp0_integer_codec.hpp"

namespace cp0_camera_api {

int parse_integer_argument(const std::string &value, int fallback)
{
    int parsed = 0;
    return cp0_integer_codec::parse_int_with_leading_whitespace(value, parsed)
        ? parsed : fallback;
}

} // namespace cp0_camera_api
