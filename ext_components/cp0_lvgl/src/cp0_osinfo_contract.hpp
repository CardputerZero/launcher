#pragma once

#include "cp0_lvgl_app.h"

#include <cstdint>
#include <ctime>
#include <functional>
#include <list>
#include <string>

namespace cp0::osinfo {

struct Result {
    int code = -1;
    std::string payload;
};

struct Operations {
    std::function<int(bool, cp0_eth_info_t *)> read_eth_info;
    std::function<int(cp0_netif_info_t *, int, int *)> list_networks;
    std::function<int(cp0_account_info_t *)> read_account_info;
    std::function<int(const std::string &)> set_time;
    std::function<int(std::tm *)> read_local_time;
    std::function<uint32_t()> random_u32;
    std::function<int()> get_ntp;
    std::function<int(bool)> set_ntp;
    std::function<int()> apt_update_background;
    std::function<int()> update_launcher_background;
};

Result dispatch(const std::list<std::string> &arguments, const Operations &operations);

} // namespace cp0::osinfo
