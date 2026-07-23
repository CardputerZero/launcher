#include "cp0_imu_codec.hpp"

#include <cstring>
#include <type_traits>

namespace cp0::imu {

static_assert(std::is_trivially_copyable<cp0_compass_info_t>::value,
              "IMU signal payload requires a trivially copyable C structure");

std::string encode_info(const cp0_compass_info_t &info)
{
    return std::string(reinterpret_cast<const char *>(&info), sizeof(info));
}

bool decode_info(const std::string &payload, cp0_compass_info_t &info)
{
    if (payload.size() != sizeof(info))
        return false;

    cp0_compass_info_t decoded{};
    std::memcpy(&decoded, payload.data(), sizeof(decoded));
    info = decoded;
    return true;
}

} // namespace cp0::imu
