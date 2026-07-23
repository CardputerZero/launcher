#include "cp0_osinfo_codec.hpp"

#include <cassert>
#include <cstring>
#include <string>

namespace {

void test_eth_round_trip_and_field_order()
{
    cp0_eth_info_t source{};
    std::strcpy(source.ipv4, "192.0.2.4/24");
    std::strcpy(source.gateway, "192.0.2.1");
    std::strcpy(source.mac, "02:00:00:00:00:04");

    const std::string payload = cp0::osinfo::encode_eth_info(source);
    assert(payload == "192.0.2.4/24\n192.0.2.1\n02:00:00:00:00:04");

    cp0_eth_info_t decoded{};
    cp0::osinfo::decode_eth_info(payload, &decoded);
    assert(std::strcmp(decoded.ipv4, source.ipv4) == 0);
    assert(std::strcmp(decoded.gateway, source.gateway) == 0);
    assert(std::strcmp(decoded.mac, source.mac) == 0);
}

void test_eth_missing_fields_use_legacy_defaults()
{
    cp0_eth_info_t info{};
    cp0::osinfo::decode_eth_info("198.51.100.8", &info);
    assert(std::strcmp(info.ipv4, "198.51.100.8") == 0);
    assert(std::strcmp(info.gateway, "N/A") == 0);
    assert(std::strcmp(info.mac, "N/A") == 0);

    cp0::osinfo::decode_eth_info("\n\n", &info);
    assert(info.ipv4[0] == '\0');
    assert(info.gateway[0] == '\0');
    assert(std::strcmp(info.mac, "N/A") == 0);
    cp0::osinfo::decode_eth_info("ignored", nullptr);
}

void test_decoders_truncate_and_terminate_fields()
{
    cp0_eth_info_t eth{};
    cp0::osinfo::decode_eth_info(std::string(200, 'i') + "\n" + std::string(200, 'g') + "\n" +
                                     std::string(200, 'm'),
                                 &eth);
    assert(std::strlen(eth.ipv4) == sizeof(eth.ipv4) - 1);
    assert(std::strlen(eth.gateway) == sizeof(eth.gateway) - 1);
    assert(std::strlen(eth.mac) == sizeof(eth.mac) - 1);

    cp0_account_info_t account{};
    cp0::osinfo::decode_account_info(std::string(200, 'u') + "\n" + std::string(300, 'h'), &account);
    assert(std::strlen(account.user) == sizeof(account.user) - 1);
    assert(std::strlen(account.hostname) == sizeof(account.hostname) - 1);
}

void test_strict_decoders_reject_malformed_payloads()
{
    cp0_eth_info_t eth{};
    assert(cp0::osinfo::decode_eth_info_strict("ip\ngateway\nmac", &eth));
    assert(std::strcmp(eth.mac, "mac") == 0);
    assert(cp0::osinfo::decode_eth_info_strict("\n\n", &eth));
    assert(!cp0::osinfo::decode_eth_info_strict("ip\ngateway", &eth));
    assert(!cp0::osinfo::decode_eth_info_strict("ip\ngateway\nmac\njunk", &eth));
    assert(!cp0::osinfo::decode_eth_info_strict("ip\ngateway\nmac", nullptr));

    cp0_account_info_t account{};
    assert(cp0::osinfo::decode_account_info_strict("user\nhost", &account));
    assert(!cp0::osinfo::decode_account_info_strict("user", &account));
    assert(!cp0::osinfo::decode_account_info_strict("user\nhost\njunk", &account));
    assert(!cp0::osinfo::decode_account_info_strict("user\nhost", nullptr));
}

void test_account_payload_compatibility()
{
    cp0_account_info_t source{};
    std::strcpy(source.user, "operator");
    std::strcpy(source.hostname, "cardputer-zero");
    assert(cp0::osinfo::encode_account_info(source) == "operator\ncardputer-zero");

    cp0_account_info_t decoded{};
    std::memset(&decoded, 0x7f, sizeof(decoded));
    cp0::osinfo::decode_account_info("operator", &decoded);
    assert(std::strcmp(decoded.user, "operator") == 0);
    assert(decoded.hostname[0] == '\0');
    cp0::osinfo::decode_account_info("ignored", nullptr);
}

void test_network_list_payload_order()
{
    cp0_netif_info_t entries[2]{};
    std::strcpy(entries[0].iface, "eth0");
    std::strcpy(entries[0].ipv4, "192.0.2.4");
    std::strcpy(entries[0].netmask, "255.255.255.0");
    entries[0].is_up = 1;
    std::strcpy(entries[1].iface, "wlan0");
    entries[1].is_up = 0;

    assert(cp0::osinfo::encode_network_list(entries, 2) ==
           "eth0\t192.0.2.4\t255.255.255.0\t1\n"
           "wlan0\t\t\t0\n");
    assert(cp0::osinfo::encode_network_list(nullptr, 2).empty());
    assert(cp0::osinfo::encode_network_list(entries, 0).empty());
}

void test_local_time_payload_order()
{
    std::tm value{};
    value.tm_year = 126;
    value.tm_mon = 6;
    value.tm_mday = 22;
    value.tm_hour = 9;
    value.tm_min = 8;
    value.tm_sec = 7;
    assert(cp0::osinfo::encode_local_time(value) == "2026,7,22,9,8,7");
}

} // namespace

int main()
{
    test_eth_round_trip_and_field_order();
    test_eth_missing_fields_use_legacy_defaults();
    test_decoders_truncate_and_terminate_fields();
    test_strict_decoders_reject_malformed_payloads();
    test_account_payload_compatibility();
    test_network_list_payload_order();
    test_local_time_payload_order();
    return 0;
}
