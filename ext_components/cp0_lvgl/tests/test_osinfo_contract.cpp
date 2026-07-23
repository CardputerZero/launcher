#include "cp0_osinfo_contract.hpp"

#include <cassert>
#include <cstring>
#include <string>
#include <stdexcept>

int main()
{
    cp0::osinfo::Operations operations;
    bool default_network = false;
    operations.read_eth_info = [&](bool is_default, cp0_eth_info_t *info) {
        default_network = is_default;
        std::strcpy(info->ipv4, "192.0.2.10");
        std::strcpy(info->gateway, "192.0.2.1");
        std::strcpy(info->mac, "00:11:22:33:44:55");
        return 0;
    };
    int network_count = 1;
    operations.list_networks = [&](cp0_netif_info_t *entries, int capacity, int *count) {
        assert(capacity == 64);
        std::strcpy(entries[0].iface, "eth0");
        entries[0].is_up = 1;
        *count = network_count;
        return 0;
    };
    operations.read_account_info = [](cp0_account_info_t *info) {
        std::strcpy(info->user, "user");
        std::strcpy(info->hostname, "host");
        return 0;
    };
    std::string timestamp;
    operations.set_time = [&](const std::string &value) {
        timestamp = value;
        return 7;
    };
    operations.read_local_time = [](std::tm *value) {
        value->tm_year = 126;
        value->tm_mon = 6;
        value->tm_mday = 22;
        return 0;
    };
    operations.random_u32 = [] { return uint32_t{4294967295U}; };
    operations.get_ntp = [] { return 1; };
    bool ntp_enabled = false;
    int ntp_calls = 0;
    operations.set_ntp = [&](bool enabled) {
        ntp_enabled = enabled;
        ++ntp_calls;
        return 0;
    };
    operations.apt_update_background = [] { return 3; };
    operations.update_launcher_background = [] { return 4; };

    auto result = cp0::osinfo::dispatch({"NetworkDefaultInfoRead"}, operations);
    assert(result.code == 0 && default_network);
    assert(result.payload == "192.0.2.10\n192.0.2.1\n00:11:22:33:44:55");
    result = cp0::osinfo::dispatch({"EthInfoRead"}, operations);
    assert(result.code == 0 && !default_network);
    result = cp0::osinfo::dispatch({"NetworkList"}, operations);
    assert(result.code == 0 && result.payload == "eth0\t\t\t1\n");
    network_count = 65;
    result = cp0::osinfo::dispatch({"NetworkList"}, operations);
    assert(result.code == -1 && result.payload.empty());
    network_count = -1;
    assert(cp0::osinfo::dispatch({"NetworkList"}, operations).code == -1);

    result = cp0::osinfo::dispatch({"AccountInfoRead"}, operations);
    assert(result.code == 0 && result.payload == "user\nhost");
    result = cp0::osinfo::dispatch({"TimeSet", "2024-02-29 23:59:59"}, operations);
    assert(result.code == 7 && timestamp == "2024-02-29 23:59:59");
    assert(cp0::osinfo::dispatch({"TimeSet", ""}, operations).code == -1);
    assert(cp0::osinfo::dispatch({"TimeSet", "2023-02-29 00:00:00"}, operations).code == -1);
    assert(cp0::osinfo::dispatch({"TimeSet", "2024-01-01 00:00:00junk"}, operations).code == -1);
    assert(cp0::osinfo::dispatch({"TimeSet", "999999999999-01-01 00:00:00"}, operations).code == -1);
    assert(cp0::osinfo::dispatch({"TimeSet", "2024-01-01 24:00:00"}, operations).code == -1);

    assert(cp0::osinfo::dispatch({"LocalTime"}, operations).payload == "2026,7,22,0,0,0");
    assert(cp0::osinfo::dispatch({"RandomU32"}, operations).payload == "4294967295");
    assert(cp0::osinfo::dispatch({"NtpGet"}, operations).code == 1);
    assert(cp0::osinfo::dispatch({"NtpSet", "1"}, operations).code == 0);
    assert(ntp_enabled && ntp_calls == 1);
    assert(cp0::osinfo::dispatch({"NtpSet", "0"}, operations).code == 0);
    assert(!ntp_enabled && ntp_calls == 2);
    assert(cp0::osinfo::dispatch({"NtpSet"}, operations).code == -1);
    assert(cp0::osinfo::dispatch({"NtpSet", "2"}, operations).code == -1);
    assert(cp0::osinfo::dispatch({"NtpSet", "1junk"}, operations).code == -1);
    assert(ntp_calls == 2);

    assert(cp0::osinfo::dispatch({"AptUpdateBackground"}, operations).code == 3);
    assert(cp0::osinfo::dispatch({"UpdateLauncherBackground"}, operations).code == 4);
    assert(cp0::osinfo::dispatch({}, operations).code == -1);
    assert(cp0::osinfo::dispatch({"Unknown"}, operations).payload == "unknown osinfo api command");

    cp0::osinfo::Operations missing;
    assert(cp0::osinfo::dispatch({"RandomU32"}, missing).code == -1);

    operations.random_u32 = []() -> uint32_t { throw std::runtime_error("backend"); };
    result = cp0::osinfo::dispatch({"RandomU32"}, operations);
    assert(result.code == -1 && result.payload == "osinfo backend failure");
    operations.list_networks = [](cp0_netif_info_t *, int, int *) -> int {
        throw std::runtime_error("backend");
    };
    result = cp0::osinfo::dispatch({"NetworkList"}, operations);
    assert(result.code == -1 && result.payload == "osinfo backend failure");
}
