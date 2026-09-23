/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

// Exercise the production backend with command execution replaced below.
#include "../src/cp0/cp0_lvgl_network.cpp"

#include <cassert>
#include <iostream>
#include <unistd.h>

decltype(cp0_signal_wifi_api) cp0_signal_wifi_api;

namespace {
struct Commands {
    std::string device = "wlan0:wifi:disconnected:--\n";
    std::string after = "wlan0:wifi:connected:NewNetwork\n";
    std::string error;
    int activation_result = 0;
    int disconnect_result = 0;
    int deletion_result = 0;
    int activations = 0;
    int disconnects = 0;
    int deletions = 0;
    std::vector<std::string> activation_args;
    std::vector<std::pair<int, std::string>> activation_results;
    std::vector<std::string> operations;
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
        commands.operations.push_back("activate");
        commands.activation_args = args;
        if (!commands.activation_results.empty()) {
            assert(static_cast<std::size_t>(commands.activations) <= commands.activation_results.size());
            const auto &result = commands.activation_results[commands.activations - 1];
            if (result.first == 0) commands.device = commands.after;
            output = result.second;
            return result.first;
        }
        commands.device = commands.after;
        output = commands.error;
        return commands.activation_result;
    } else if (args == std::vector<std::string>{"nmcli", "dev", "disconnect", "wlan0"}) {
        ++commands.disconnects;
        if (commands.disconnect_result == 0)
            commands.device = "wlan0:wifi:disconnected:--\n";
        return commands.disconnect_result;
    } else if (args == std::vector<std::string>{"nmcli", "-t", "--escape", "no", "-f", "UUID,TYPE,NAME", "con", "show"}) {
        output = "test-uuid:802-11-wireless:netplan-wlan0-NewNetwork\n"
                 "old-uuid:802-11-wireless:OldNetwork\n"
                 "wired-uuid:802-3-ethernet:Wired connection 1\n";
    } else if (args == std::vector<std::string>{"nmcli", "-t", "--escape", "no", "-g", "802-11-wireless.ssid", "con", "show", "uuid", "test-uuid"}) {
        output = "NewNetwork\n";
    } else if (args == std::vector<std::string>{"nmcli", "-t", "--escape", "no", "-g", "802-11-wireless.ssid", "con", "show", "uuid", "old-uuid"}) {
        output = "OldNetwork\n";
    } else if (args == std::vector<std::string>{"nmcli", "con", "delete", "uuid", "test-uuid"}) {
        ++commands.deletions;
        commands.operations.push_back("delete-target");
        return commands.deletion_result;
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
        const std::string missing_key =
            "Error: Failed to modify connection: 802-11-wireless-security.key-mgmt: property is missing";
        commands = {};
        commands.device = "wlan0:wifi:connected:OldNetwork\n";
        commands.activation_results = {{10, missing_key}, {0, "Connection successfully activated"}};
        assert(wifi.connect("NewNetwork", "newpassword", hidden) == 0);
        assert(commands.activations == 2 && commands.deletions == 1 && commands.disconnects == 0);
        assert((commands.operations == std::vector<std::string>{"activate", "delete-target", "activate"}));
        std::vector<std::string> recovered_args = {
            "nmcli", "--wait", "20", "dev", "wifi", "connect", "NewNetwork", "password", "newpassword"};
        if (hidden) recovered_args.insert(recovered_args.end(), {"hidden", "yes"});
        assert(commands.activation_args == recovered_args);
        assert(wifi.get_status().connected);

        // Recovery is bounded even when the recreated profile has the same error.
        commands = {};
        commands.activation_results = {{10, missing_key}, {10, missing_key}};
        assert(wifi.connect("NewNetwork", "newpassword", hidden) == CP0_WIFI_ERROR_SERVICE);
        assert(commands.activations == 2 && commands.deletions == 2);

        commands = {};
        commands.activation_results = {{10, missing_key}, {10, "Authentication failed"}};
        assert(wifi.connect("NewNetwork", "wrongpass", hidden) == CP0_WIFI_ERROR_AUTH);
        assert(commands.activations == 2 && commands.deletions == 2);

        commands = {};
        commands.activation_results = {{10, missing_key}};
        commands.deletion_result = CP0_WIFI_ERROR_SERVICE;
        assert(wifi.connect("NewNetwork", "newpassword", hidden) == CP0_WIFI_ERROR_SERVICE);
        assert(commands.activations == 1 && commands.deletions == 1);

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

    // Preserve saved credentials when the caller did not provide a replacement password.
    commands = {};
    commands.activation_result = 10;
    commands.error = "802-11-wireless-security.key-mgmt: property is missing";
    assert(wifi.connect("NewNetwork", nullptr, true) == CP0_WIFI_ERROR_SERVICE);
    assert(commands.activations == 1 && commands.deletions == 0);

    // A switch failure must retain the raw exit code while keeping echoed secrets out of stderr.
    FILE *captured = std::tmpfile();
    assert(captured);
    std::fflush(stderr);
    const int saved_stderr = dup(STDERR_FILENO);
    assert(saved_stderr >= 0);
    assert(dup2(fileno(captured), STDERR_FILENO) >= 0);
    commands = {};
    commands.device = "wlan0:wifi:connected:OldNetwork\n";
    commands.activation_result = 3;
    commands.error = "Timeout expired\nsecret=test-secret-for-log; repeated=test-secret-for-log";
    assert(wifi.connect("NewNetwork", "test-secret-for-log", true) == CP0_WIFI_ERROR_SERVICE);
    assert(commands.activations == 1 && commands.disconnects == 0 && commands.deletions == 1);
    std::fflush(stderr);
    assert(dup2(saved_stderr, STDERR_FILENO) >= 0);
    close(saved_stderr);
    std::rewind(captured);
    std::string logs;
    char buffer[1024];
    while (const std::size_t count = std::fread(buffer, 1, sizeof(buffer), captured))
        logs.append(buffer, count);
    std::fclose(captured);
    assert(logs.find("[wifi-connect] id=") != std::string::npos);
    assert(logs.find("ssid=\"OldNetwork\"") != std::string::npos);
    assert(logs.find("stage=activate-end rc=3") != std::string::npos);
    assert(logs.find("command_ms=") != std::string::npos);
    assert(logs.find("Timeout expired\\nsecret=<redacted>; repeated=<redacted>") != std::string::npos);
    assert(logs.find("test-secret-for-log") == std::string::npos);
    assert(logs.find("stage=cleanup-end rc=0") != std::string::npos);
    assert(logs.find("stage=complete rc=" + std::to_string(CP0_WIFI_ERROR_SERVICE)) != std::string::npos);
    std::cout << "Wi-Fi connection tests passed\n";
}
