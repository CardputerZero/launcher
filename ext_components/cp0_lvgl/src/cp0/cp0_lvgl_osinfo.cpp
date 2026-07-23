#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../cp0_app_internal_utils.h"
#include "../cp0_callback_contract.hpp"
#include "../cp0_osinfo_codec.hpp"
#include "../cp0_osinfo_contract.hpp"
#include "../cp0_signal_registration.hpp"
#include "../cp0_sync_signal.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <random>
#include <pwd.h>
#include <sstream>
#include <string>
#include <utility>
#include <unistd.h>

class OsInfoSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    OsInfoSystem()
    {
        operations_.read_eth_info = [](bool, cp0_eth_info_t *info) {
            return network_default_info_read(info);
        };
        operations_.list_networks = [](cp0_netif_info_t *entries, int capacity, int *count) {
            return cp0_network_list(entries, capacity, count);
        };
        operations_.read_account_info = account_info_read;
        operations_.set_time = [](const std::string &timestamp) { return time_set(timestamp.c_str()); };
        operations_.read_local_time = read_local_time;
        operations_.random_u32 = random_u32;
        operations_.get_ntp = ntp_get;
        operations_.set_ntp = ntp_set;
        operations_.apt_update_background = apt_update_background;
        operations_.update_launcher_background = update_launcher_background;
    }

    void api_call(arg_t arg, callback_t callback)
    {
        const cp0::osinfo::Result result = cp0::osinfo::dispatch(arg, operations_);
        cp0::callback::invoke(callback, result.code, result.payload);
    }

    static int api_simple(const arg_t &arg, std::string *out = nullptr)
    {
        int result = -1;
        if (out) out->clear();
        cp0::signal::invoke_noexcept([&] { cp0_signal_osinfo_api(arg, [&](int code, std::string data) {
            result = code;
            if (out)
                *out = std::move(data);
        }); });
        return result;
    }

private:
    cp0::osinfo::Operations operations_;

    static uint32_t random_u32() noexcept
    {
        try {
            std::random_device source;
            return static_cast<uint32_t>(source());
        } catch (...) {
            const auto seed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
            static thread_local std::mt19937 fallback(seed);
            return fallback();
        }
    }

    static int read_local_time(std::tm *value)
    {
        if (!value)
            return -1;
        const std::time_t now = std::time(nullptr);
        return localtime_r(&now, value) ? 0 : (errno ? -errno : -1);
    }

    static int network_default_info_read(cp0_eth_info_t *info)
    {
        cp0::osinfo::clear_eth_info(info);
        if (!info)
            return -1;

        char output[2048] = {};
        const char *ip_argv[] = {"ip", "-4", "addr", "show", "eth0", nullptr};
        if (cp0_process_capture_argv(ip_argv, output, sizeof(output)) == 0) {
            std::istringstream lines(output);
            std::string line;
            while (std::getline(lines, line)) {
                auto pos = line.find("inet ");
                if (pos == std::string::npos)
                    continue;
                std::istringstream iss(line.substr(pos + 5));
                std::string ip;
                iss >> ip;
                cp0_copy_string(info->ipv4, sizeof(info->ipv4), ip.empty() ? "N/A" : ip);
                break;
            }
        }

        const char *route_argv[] = {"ip", "route", nullptr};
        if (cp0_process_capture_argv(route_argv, output, sizeof(output)) == 0) {
            std::istringstream lines(output);
            std::string line;
            while (std::getline(lines, line)) {
                if (line.find("default") == std::string::npos || line.find("eth0") == std::string::npos)
                    continue;
                std::istringstream iss(line);
                std::string word;
                while (iss >> word) {
                    if (word == "via") {
                        std::string gw;
                        if (iss >> gw)
                            cp0_copy_string(info->gateway, sizeof(info->gateway), gw);
                        break;
                    }
                }
                break;
            }
        }

        cp0_file_read_first_line("/sys/class/net/eth0/address", info->mac, sizeof(info->mac));
        return 0;
    }

    static int account_info_read(cp0_account_info_t *info)
    {
        if (!info)
            return -1;
        std::memset(info, 0, sizeof(*info));
        const char *user = getlogin();
        if (!user || !user[0]) {
            struct passwd *pw = getpwuid(getuid());
            user = pw ? pw->pw_name : nullptr;
        }
        cp0_copy_cstr(info->user, sizeof(info->user), user && user[0] ? user : "N/A");

        char host[sizeof(info->hostname)] = {};
        if (gethostname(host, sizeof(host) - 1) == 0 && host[0])
            cp0_copy_cstr(info->hostname, sizeof(info->hostname), host);
        else
            cp0_copy_cstr(info->hostname, sizeof(info->hostname), "N/A");
        return 0;
    }

    static int time_set(const char *timestamp)
    {
        if (!timestamp || !timestamp[0])
            return -1;
        const char *argv[] = {"sudo", "date", "-s", timestamp, nullptr};
        return cp0_process_run_argv(argv, 0);
    }

    // Returns 1 if systemd automatic time sync (NTP) is enabled, 0 if disabled,
    // negative on failure to query.
    static int ntp_get()
    {
        char output[64] = {};
        const char *argv[] = {"timedatectl", "show", "-p", "NTP", "--value", nullptr};
        if (cp0_process_capture_argv(argv, output, sizeof(output)) != 0)
            return -1;
        std::string s(output);
        // trim trailing newline / whitespace
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
            s.pop_back();
        return s == "yes" ? 1 : 0;
    }

    static int ntp_set(bool enable)
    {
        const char *argv[] = {"sudo", "timedatectl", "set-ntp", enable ? "true" : "false", nullptr};
        return cp0_process_run_argv(argv, 0);
    }

    static int apt_update_background()
    {
        const char *argv[] = {"apt", "update", nullptr};
        return cp0_process_run_argv(argv, 1);
    }

    static int update_launcher_background()
    {
        const char *argv[] = {
            "sh", "-c",
            "cd /usr/share/APPLaunch && "
            "wget -q https://github.com/CardputerZero/M5CardputerZero-Launcher/releases/latest/download/applaunch_*.deb -O /tmp/launcher_update.deb 2>/dev/null && "
            "dpkg -i /tmp/launcher_update.deb >/dev/null 2>&1 && "
            "systemctl restart APPLaunch",
            nullptr
        };
        return cp0_process_run_argv(argv, 1);
    }

public:
    static int api_eth_info(const char *command, cp0_eth_info_t *info)
    {
        if (!info)
            return -1;
        std::string data;
        int ret = api_simple({command}, &data);
        if (ret == 0 && !cp0::osinfo::decode_eth_info_strict(data, info))
            ret = -1;
        return ret;
    }

    static int api_account_info(cp0_account_info_t *info)
    {
        if (!info)
            return -1;
        std::string data;
        int ret = api_simple({"AccountInfoRead"}, &data);
        if (ret == 0 && !cp0::osinfo::decode_account_info_strict(data, info))
            ret = -1;
        return ret;
    }
};

extern "C" void init_osinfo(void)
{
    static cp0::SignalRegistration<decltype(cp0_signal_osinfo_api)> registration;
    auto osinfo = std::make_shared<OsInfoSystem>();
    registration.replace(cp0_signal_osinfo_api, [osinfo](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        osinfo->api_call(std::move(arg), std::move(callback));
    });
}

extern "C" int cp0_network_default_info_read(cp0_eth_info_t *info)
{
    return OsInfoSystem::api_eth_info("NetworkDefaultInfoRead", info);
}

extern "C" int cp0_eth_info_read(cp0_eth_info_t *info)
{
    return OsInfoSystem::api_eth_info("EthInfoRead", info);
}

extern "C" int cp0_account_info_read(cp0_account_info_t *info)
{
    return OsInfoSystem::api_account_info(info);
}

extern "C" int cp0_time_set(const char *timestamp)
{
    return OsInfoSystem::api_simple({"TimeSet", timestamp ? timestamp : ""});
}

extern "C" int cp0_time_ntp_get(void)
{
    return OsInfoSystem::api_simple({"NtpGet"});
}

extern "C" int cp0_time_ntp_set(int enable)
{
    return OsInfoSystem::api_simple({"NtpSet", enable ? "1" : "0"});
}

extern "C" int cp0_system_apt_update_background(void)
{
    return OsInfoSystem::api_simple({"AptUpdateBackground"});
}

extern "C" int cp0_system_update_launcher_background(void)
{
    return OsInfoSystem::api_simple({"UpdateLauncherBackground"});
}
