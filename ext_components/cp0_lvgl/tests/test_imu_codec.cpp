#include "cp0_imu_codec.hpp"
#include "cp0_callback_contract.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

cp0_compass_info_t sample_info()
{
    cp0_compass_info_t info{};
    std::strcpy(info.status, "Sensor OK");
    info.yaw = 123.5f;
    info.pitch = -4.25f;
    info.roll = 8.0f;
    info.acc_x = 1.0f;
    info.acc_y = 2.0f;
    info.acc_z = 3.0f;
    info.gyr_x = 4.0f;
    info.gyr_y = 5.0f;
    info.gyr_z = 6.0f;
    info.mag_x = 7.0f;
    info.mag_y = 8.0f;
    info.mag_z = 9.0f;
    info.sensor_ready = 1;
    return info;
}

void test_round_trip_preserves_binary_payload()
{
    const cp0_compass_info_t expected = sample_info();
    const std::string payload = cp0::imu::encode_info(expected);
    assert(payload.size() == sizeof(expected));

    cp0_compass_info_t actual{};
    assert(cp0::imu::decode_info(payload, actual));
    assert(std::memcmp(&actual, &expected, sizeof(expected)) == 0);
}

void test_decode_rejects_wrong_size_without_mutating_output()
{
    const cp0_compass_info_t unchanged = sample_info();
    cp0_compass_info_t output = unchanged;

    assert(!cp0::imu::decode_info({}, output));
    assert(std::memcmp(&output, &unchanged, sizeof(output)) == 0);

    const std::string payload = cp0::imu::encode_info(unchanged);
    assert(!cp0::imu::decode_info(payload.substr(0, payload.size() - 1), output));
    assert(std::memcmp(&output, &unchanged, sizeof(output)) == 0);

    assert(!cp0::imu::decode_info(payload + '\0', output));
    assert(std::memcmp(&output, &unchanged, sizeof(output)) == 0);
}

} // namespace

int main()
{
    int callback_calls = 0;
    cp0::callback::invoke_direct([&](int value) {
        ++callback_calls;
        assert(value == 7);
        throw std::runtime_error("callback");
    }, 7);
    assert(callback_calls == 1);
    void (*empty_callback)(int) = nullptr;
    cp0::callback::invoke_direct(empty_callback, 7);
    assert(callback_calls == 1);

    test_round_trip_preserves_binary_payload();
    test_decode_rejects_wrong_size_without_mutating_output();
    return 0;
}
