#pragma once

#include <string>
#include <vector>

namespace cp0::network {

struct AccessPoint
{
    std::string ssid;
    int signal = 0;
    std::string security;
    bool in_use = false;
};

struct Status
{
    bool connected = false;
    bool ethernet = false;
    std::string wifi_interface;
    std::string ssid;
    std::string ipv4;
    int signal = 0;
};

std::vector<std::string> split_terse_fields(const std::string &line);
std::vector<AccessPoint> parse_scan_output(const std::string &output);
Status parse_device_status(const std::string &output);
void apply_active_wifi(const std::string &output, Status &status);
std::string parse_ipv4_address(const std::string &output);
bool parse_radio_enabled(const std::string &output);
std::string first_active_connection(const std::string &output);

} // namespace cp0::network
