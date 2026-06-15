#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../cp0_app_internal_utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
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

    void api_call(arg_t arg, callback_t callback)
    {
        const std::string cmd = nth_arg(arg, 0);
        if (cmd == "NetworkDefaultInfoRead" || cmd == "EthInfoRead") {
            cp0_eth_info_t info{};
            int ret = network_default_info_read(&info);
            report(callback, ret, encode_eth_info(info));
        } else if (cmd == "AccountInfoRead") {
            cp0_account_info_t info{};
            int ret = account_info_read(&info);
            report(callback, ret, encode_account_info(info));
        } else if (cmd == "TimeSet") {
            report(callback, time_set(nth_arg(arg, 1).c_str()), "");
        } else if (cmd == "AptUpdateBackground") {
            report(callback, apt_update_background(), "");
        } else if (cmd == "UpdateLauncherBackground") {
            report(callback, update_launcher_background(), "");
        } else {
            report(callback, -1, "unknown osinfo api command");
        }
    }

    static int api_simple(const arg_t &arg, std::string *out = nullptr)
    {
        int result = -1;
        cp0_signal_osinfo_api(arg, [&](int code, std::string data) {
            result = code;
            if (out)
                *out = std::move(data);
        });
        return result;
    }

private:
    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        std::advance(it, std::min(index, arg.size()));
        return it == arg.end() ? std::string() : *it;
    }

    static void clear_net_info(cp0_eth_info_t *info)
    {
        if (!info)
            return;
        std::memset(info, 0, sizeof(*info));
        cp0_copy_cstr(info->ipv4, sizeof(info->ipv4), "N/A");
        cp0_copy_cstr(info->gateway, sizeof(info->gateway), "N/A");
        cp0_copy_cstr(info->mac, sizeof(info->mac), "N/A");
    }

    static int network_default_info_read(cp0_eth_info_t *info)
    {
        clear_net_info(info);
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

    static std::string encode_eth_info(const cp0_eth_info_t &info)
    {
        return std::string(info.ipv4) + "\n" + info.gateway + "\n" + info.mac;
    }

    static void decode_eth_info(const std::string &data, cp0_eth_info_t *info)
    {
        clear_net_info(info);
        if (!info)
            return;
        std::istringstream lines(data);
        std::string line;
        if (std::getline(lines, line)) cp0_copy_string(info->ipv4, sizeof(info->ipv4), line);
        if (std::getline(lines, line)) cp0_copy_string(info->gateway, sizeof(info->gateway), line);
        if (std::getline(lines, line)) cp0_copy_string(info->mac, sizeof(info->mac), line);
    }

    static std::string encode_account_info(const cp0_account_info_t &info)
    {
        return std::string(info.user) + "\n" + info.hostname;
    }

    static void decode_account_info(const std::string &data, cp0_account_info_t *info)
    {
        if (!info)
            return;
        std::memset(info, 0, sizeof(*info));
        std::istringstream lines(data);
        std::string line;
        if (std::getline(lines, line)) cp0_copy_string(info->user, sizeof(info->user), line);
        if (std::getline(lines, line)) cp0_copy_string(info->hostname, sizeof(info->hostname), line);
    }

public:
    static int api_eth_info(const char *command, cp0_eth_info_t *info)
    {
        if (!info)
            return -1;
        std::string data;
        int ret = api_simple({command}, &data);
        if (ret == 0)
            decode_eth_info(data, info);
        return ret;
    }

    static int api_account_info(cp0_account_info_t *info)
    {
        if (!info)
            return -1;
        std::string data;
        int ret = api_simple({"AccountInfoRead"}, &data);
        if (ret == 0)
            decode_account_info(data, info);
        return ret;
    }
};

extern "C" void init_osinfo(void)
{
    auto osinfo = std::make_shared<OsInfoSystem>();
    cp0_signal_osinfo_api.append([osinfo](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
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

extern "C" int cp0_system_apt_update_background(void)
{
    return OsInfoSystem::api_simple({"AptUpdateBackground"});
}

extern "C" int cp0_system_update_launcher_background(void)
{
    return OsInfoSystem::api_simple({"UpdateLauncherBackground"});
}
