/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

// Exercise the production backend with command execution replaced below.
#include "../src/cp0/cp0_lvgl_network.cpp"

#include <cassert>
#include <iostream>

decltype(cp0_signal_wifi_api) cp0_signal_wifi_api;

namespace {
struct Commands {
    std::string device = "wlan0:wifi:disconnected:--\n";
    std::string after = "wlan0:wifi:connected:NewNetwork\n";
    std::string error;
    int activation_result = 0;
    int disconnect_result = 0;
    int activations = 0;
    int disconnects = 0;
    int deletions = 0;
    std::vector<std::string> activation_args;
} commands;
}

int cp0_process_commands::capture_argv_with_timeout(
    const std::vector<std::string> &args, std::string &output, int,
    const std::atomic<bool> *)
{
    output.clear();
    if (args == std::vector<std::string>{"nmcli", "-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "dev", "status"}) {
        output = commands.device;
    } else if (args.size() > 1 && args[1] == "--wait") {
        ++commands.activations;
        commands.activation_args = args;
        commands.device = commands.after;
        output = commands.error;
        return commands.activation_result;
    } else if (args == std::vector<std::string>{"nmcli", "dev", "disconnect", "wlan0"}) {
        ++commands.disconnects;
        if (commands.disconnect_result == 0)
            commands.device = "wlan0:wifi:disconnected:--\n";
        return commands.disconnect_result;
    } else if (args == std::vector<std::string>{"nmcli", "-t", "--escape", "no", "-f", "UUID,TYPE,NAME", "con", "show"}) {
        output = "test-uuid:802-11-wireless:NewNetwork\n";
    } else if (args == std::vector<std::string>{"nmcli", "con", "delete", "uuid", "test-uuid"}) {
        ++commands.deletions;
    } else if (args == std::vector<std::string>{"nmcli", "-t", "-f", "IN-USE,SIGNAL,SSID", "dev", "wifi", "list", "--rescan", "no"}) {
        output = "*:70:NewNetwork\n";
    } else if (args == std::vector<std::string>{"ip", "-4", "-o", "addr", "show", "wlan0"}) {
        output = "3: wlan0 inet 192.168.1.42/24\n";
    } else {
        assert(false && "unexpected command");
    }
    return 0;
}

int main()
{
    WifiSystem wifi;
    // Keep status refresh deterministic after the production worker is joined.
    wifi.stop();

    for (bool hidden : {false, true}) {
        commands = {};
        commands.activation_result = 10;
        commands.error = "Secrets were required, but not provided";
        commands.after = "wlan0:wifi:connecting (need authentication):NewNetwork\n";
        assert(wifi.connect("NewNetwork", "wrongpass", hidden) == CP0_WIFI_ERROR_AUTH);
        assert(!wifi.get_status().connected);
        assert(commands.activations == 1);
        assert(commands.deletions == 1);

        // A stale snapshot or automatic reconnection must not erase a failure.
        commands = {};
        commands.activation_result = 10;
        commands.error = "Authentication failed";
        assert(wifi.connect("NewNetwork", "wrongpass", hidden) == CP0_WIFI_ERROR_AUTH);
        assert(commands.activations == 1);

        commands = {};
        commands.device = commands.after;
        commands.activation_result = 10;
        commands.error = "Authentication failed";
        assert(wifi.connect("NewNetwork", "wrongpass", hidden) == CP0_WIFI_ERROR_AUTH);
        assert(commands.disconnects == 1);
        assert(commands.activations == 1);

        commands = {};
        commands.activation_result = -ETIMEDOUT;
        assert(wifi.connect("NewNetwork", "wrongpass", hidden) == CP0_WIFI_ERROR_TIMEOUT);

        commands = {};
        commands.after = "wlan0:wifi:connecting (configuring):NewNetwork\n";
        assert(wifi.connect("NewNetwork", "password", hidden) != 0);

        commands = {};
        commands.after = "wlan0:wifi:connected:OtherNetwork\n";
        assert(wifi.connect("NewNetwork", "password", hidden) != 0);

        commands = {};
        commands.device = commands.after;
        assert(wifi.connect("NewNetwork", "newpassword", hidden) == 0);
        assert(commands.disconnects == 1);
        assert(commands.activations == 1);
        std::vector<std::string> expected = {
            "nmcli", "--wait", "20", "dev", "wifi", "connect", "NewNetwork", "password", "newpassword"};
        if (hidden) expected.insert(expected.end(), {"hidden", "yes"});
        assert(commands.activation_args == expected);
        assert(wifi.connect("NewNetwork", "newpassword", hidden) == 0);
        assert(commands.disconnects == 2);
        assert(commands.activations == 2);

        commands = {};
        commands.device = commands.after;
        commands.disconnect_result = CP0_WIFI_ERROR_SERVICE;
        assert(wifi.connect("NewNetwork", "newpassword", hidden) == CP0_WIFI_ERROR_SERVICE);
        assert(commands.activations == 0);

        commands = {};
        assert(wifi.connect("NewNetwork", nullptr, hidden) == 0);
        assert(commands.activations == 1);
    }
    std::cout << "Wi-Fi connection tests passed\n";
}
