#pragma once

#include "cp0_lvgl_app.h"

#include <ctime>
#include <string>

namespace cp0::osinfo {

void clear_eth_info(cp0_eth_info_t *info);
std::string encode_eth_info(const cp0_eth_info_t &info);
void decode_eth_info(const std::string &payload, cp0_eth_info_t *info);
bool decode_eth_info_strict(const std::string &payload, cp0_eth_info_t *info);

std::string encode_account_info(const cp0_account_info_t &info);
void decode_account_info(const std::string &payload, cp0_account_info_t *info);
bool decode_account_info_strict(const std::string &payload, cp0_account_info_t *info);

std::string encode_network_list(const cp0_netif_info_t *entries, int count);
std::string encode_local_time(const std::tm &value);

} // namespace cp0::osinfo
