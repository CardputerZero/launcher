#include "cp0_osinfo_codec.hpp"

#include "cp0_app_internal_utils.h"

#include <cstring>
#include <sstream>

namespace cp0::osinfo {

void clear_eth_info(cp0_eth_info_t *info)
{
    if (!info)
        return;
    std::memset(info, 0, sizeof(*info));
    cp0_copy_cstr(info->ipv4, sizeof(info->ipv4), "N/A");
    cp0_copy_cstr(info->gateway, sizeof(info->gateway), "N/A");
    cp0_copy_cstr(info->mac, sizeof(info->mac), "N/A");
}

std::string encode_eth_info(const cp0_eth_info_t &info)
{
    return std::string(info.ipv4) + "\n" + info.gateway + "\n" + info.mac;
}

void decode_eth_info(const std::string &payload, cp0_eth_info_t *info)
{
    clear_eth_info(info);
    if (!info)
        return;

    std::istringstream lines(payload);
    std::string line;
    if (std::getline(lines, line))
        cp0_copy_string(info->ipv4, sizeof(info->ipv4), line);
    if (std::getline(lines, line))
        cp0_copy_string(info->gateway, sizeof(info->gateway), line);
    if (std::getline(lines, line))
        cp0_copy_string(info->mac, sizeof(info->mac), line);
}

bool decode_eth_info_strict(const std::string &payload, cp0_eth_info_t *info)
{
    clear_eth_info(info);
    if (!info)
        return false;
    const size_t first = payload.find('\n');
    const size_t second = first == std::string::npos ? std::string::npos : payload.find('\n', first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        payload.find('\n', second + 1) != std::string::npos)
        return false;
    cp0_copy_string(info->ipv4, sizeof(info->ipv4), payload.substr(0, first));
    cp0_copy_string(info->gateway, sizeof(info->gateway), payload.substr(first + 1, second - first - 1));
    cp0_copy_string(info->mac, sizeof(info->mac), payload.substr(second + 1));
    return true;
}

std::string encode_account_info(const cp0_account_info_t &info)
{
    return std::string(info.user) + "\n" + info.hostname;
}

void decode_account_info(const std::string &payload, cp0_account_info_t *info)
{
    if (!info)
        return;
    std::memset(info, 0, sizeof(*info));

    std::istringstream lines(payload);
    std::string line;
    if (std::getline(lines, line))
        cp0_copy_string(info->user, sizeof(info->user), line);
    if (std::getline(lines, line))
        cp0_copy_string(info->hostname, sizeof(info->hostname), line);
}

bool decode_account_info_strict(const std::string &payload, cp0_account_info_t *info)
{
    if (!info)
        return false;
    std::memset(info, 0, sizeof(*info));
    const size_t separator = payload.find('\n');
    if (separator == std::string::npos || payload.find('\n', separator + 1) != std::string::npos)
        return false;
    cp0_copy_string(info->user, sizeof(info->user), payload.substr(0, separator));
    cp0_copy_string(info->hostname, sizeof(info->hostname), payload.substr(separator + 1));
    return true;
}

std::string encode_network_list(const cp0_netif_info_t *entries, int count)
{
    std::ostringstream out;
    for (int i = 0; entries && i < count; ++i) {
        out << entries[i].iface << '\t' << entries[i].ipv4 << '\t'
            << entries[i].netmask << '\t' << entries[i].is_up << '\n';
    }
    return out.str();
}

std::string encode_local_time(const std::tm &value)
{
    std::ostringstream out;
    out << value.tm_year + 1900 << ',' << value.tm_mon + 1 << ',' << value.tm_mday << ','
        << value.tm_hour << ',' << value.tm_min << ',' << value.tm_sec;
    return out.str();
}

} // namespace cp0::osinfo
