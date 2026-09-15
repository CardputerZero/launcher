/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "wizard_service.h"
#include "cp0_lvgl_app.h"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {
int result = 0;
int radio_result = 0;
int calls = 0;
bool used_hidden = false;
std::string submitted_password;
std::string status_ip = "192.168.1.42";

int connect(const char *ssid, const char *password, bool hidden)
{
    assert(std::string(ssid) == "NewNetwork");
    ++calls;
    used_hidden = hidden;
    submitted_password = password ? password : "";
    return result;
}
}

extern "C" int cp0_wifi_connect(const char *ssid, const char *password)
{
    return connect(ssid, password, false);
}

extern "C" int cp0_wifi_connect_hidden(const char *ssid, const char *password)
{
    return connect(ssid, password, true);
}

extern "C" int cp0_wifi_radio_set_enabled(int enabled)
{
    assert(enabled == 1);
    return radio_result;
}

extern "C" int cp0_wifi_status_read(cp0_wifi_status_t *status)
{
    *status = {};
    status->connected = 1;
    std::strcpy(status->ssid, "NewNetwork");
    std::strcpy(status->ip, status_ip.c_str());
    return 0;
}

int main()
{
    using launch_wizard::WizardService;
    for (bool hidden : {false, true}) {
        for (const auto &failure : {
                 std::pair{CP0_WIFI_ERROR_AUTH, "Incorrect Wi-Fi password"},
                 std::pair{CP0_WIFI_ERROR_TIMEOUT, "Wi-Fi connection timed out; retry"},
                 std::pair{CP0_WIFI_ERROR_SERVICE, "Network service could not connect; retry"}}) {
            calls = 0;
            result = failure.first;
            std::string ip = "stale IP";
            assert(WizardService::connect_wifi("NewNetwork", "wrongpass", &ip, hidden) == failure.second);
            assert(ip.empty());
            assert(calls == 1);
            assert(used_hidden == hidden);
            assert(submitted_password == "wrongpass");
        }

        result = 0;
        calls = 0;
        for (int attempt = 0; attempt < 2; ++attempt) {
            std::string ip;
            assert(WizardService::connect_wifi("NewNetwork", "newpassword", &ip, hidden).empty());
            assert(ip == "192.168.1.42");
            assert(calls == attempt + 1);
            assert(used_hidden == hidden);
            assert(submitted_password == "newpassword");
        }
        assert(WizardService::connect_wifi("NewNetwork", "", nullptr, hidden).empty());
        assert(submitted_password.empty());

        calls = 0;
        radio_result = CP0_WIFI_ERROR_SERVICE;
        assert(!WizardService::connect_wifi("NewNetwork", "password", nullptr, hidden).empty());
        assert(calls == 0);
        radio_result = 0;
    }

    // An activated link without an IPv4 address must not be reported as a
    // successful connection. This guards against the UI showing the
    // connected page with "IP: Unavailable" while DHCP is still pending.
    status_ip.clear();
    std::string no_ip;
    assert(WizardService::connect_wifi("NewNetwork", "newpassword", &no_ip) ==
           "Wi-Fi did not become active");
    assert(no_ip.empty());

    std::string ip = "stale IP";
    assert(!WizardService::connect_wifi("", "password", &ip).empty());
    assert(ip.empty());
    std::cout << "Wizard Wi-Fi connection tests passed\n";
}
