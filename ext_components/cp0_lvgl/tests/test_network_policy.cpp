#include "cp0_network_policy.hpp"

#include <cassert>
#include <iostream>

using cp0::network::AccessPoint;
using cp0::network::Status;

static void test_terse_fields_decode_escaped_delimiters()
{
    const auto fields = cp0::network::split_terse_fields(R"(office\:east:WPA2\\WPA3:*)");
    assert(fields.size() == 3);
    assert(fields[0] == "office:east");
    assert(fields[1] == "WPA2\\WPA3");
    assert(fields[2] == "*");
}

static void test_scan_deduplicates_by_decoded_ssid()
{
    const auto access_points = cp0::network::parse_scan_output(
        "office\\:east:31:WPA2: *\r\n"
        "office\\:east:72:WPA2:  \n"
        ":99:OPEN:  \n"
        "junk:not-a-number:OPEN:  \n"
        "too-high:101:OPEN:  \n"
        "guest:54:OPEN:  \n");

    assert(access_points.size() == 2);
    assert(access_points[0].ssid == "office:east");
    assert(access_points[0].signal == 72);
    assert(access_points[0].in_use);
    assert(access_points[1].ssid == "guest");
}

static void test_device_status_preserves_network_semantics()
{
    const Status status = cp0::network::parse_device_status(
        "eth0:ethernet:connected:Wired connection 1\n"
        "wlan0:wifi:connected (externally):--\n");
    assert(status.ethernet);
    assert(status.connected);
    assert(status.wifi_interface == "wlan0");
    assert(status.ssid.empty());

    const Status netplan = cp0::network::parse_device_status(
        "wlan0:wifi:connected:netplan-wlan0-home\\:lab\n");
    assert(netplan.connected);
    assert(netplan.ssid == "home:lab");
}

static void test_active_wifi_fills_signal_and_missing_ssid()
{
    Status status;
    cp0::network::apply_active_wifi(" :80:other\n*:67:home\\:lab\n", status);
    assert(status.signal == 67);
    assert(status.ssid == "home:lab");

    status.ssid = "profile name";
    cp0::network::apply_active_wifi("*:52:ignored\n", status);
    assert(status.ssid == "profile name");
    cp0::network::apply_active_wifi("*:5junk:broken\n*:101:also-broken\n", status);
    assert(status.signal == 52);
}

static void test_simple_command_output_parsers()
{
    assert(cp0::network::parse_ipv4_address("3: wlan0 inet 192.168.1.42/24 brd 192.168.1.255\n") ==
           "192.168.1.42");
    assert(cp0::network::parse_ipv4_address("no address here").empty());
    assert(cp0::network::parse_radio_enabled(" \tenabled\r\n"));
    assert(!cp0::network::parse_radio_enabled("disabled\n"));
    assert(cp0::network::first_active_connection("lo\ncoffee\\:shop\r\nbackup\n") == "coffee:shop");
}

int main()
{
    test_terse_fields_decode_escaped_delimiters();
    test_scan_deduplicates_by_decoded_ssid();
    test_device_status_preserves_network_semantics();
    test_active_wifi_fills_signal_and_missing_ssid();
    test_simple_command_output_parsers();
    std::cout << "network policy tests passed\n";
    return 0;
}
