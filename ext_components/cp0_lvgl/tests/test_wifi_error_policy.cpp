/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../src/cp0/cp0_wifi_error_policy.hpp"

#include <cassert>

int main()
{
    assert(cp0::wifi::is_missing_key_management(
        "Error: Failed to modify connection: 802-11-wireless-security.key-mgmt: property is missing"));
    assert(cp0::wifi::is_missing_key_management("802-11-wireless-security.key-mgmt: Property is missing"));
    assert(!cp0::wifi::is_missing_key_management("802-11-wireless-security.key-mgmt: unsupported value"));
    assert(!cp0::wifi::is_missing_key_management("802-11-wireless-security.psk: property is missing"));
    assert(!cp0::wifi::is_missing_key_management("Authentication failed"));
    assert(!cp0::wifi::is_missing_key_management("Timeout expired"));
    assert(cp0::wifi::classify_command_failure("Secrets were required") ==
           CP0_WIFI_ERROR_AUTH);
    assert(cp0::wifi::classify_command_failure("No network with SSID found") ==
           CP0_WIFI_ERROR_NOT_FOUND);
    assert(cp0::wifi::classify_command_failure("IP configuration timed out (DHCP)") ==
           CP0_WIFI_ERROR_IP_CONFIG);
    assert(cp0::wifi::classify_command_failure("Wi-Fi radio is disabled") ==
           CP0_WIFI_ERROR_RADIO_OFF);
    assert(cp0::wifi::classify_command_failure("NetworkManager is not running") ==
           CP0_WIFI_ERROR_SERVICE);
}
