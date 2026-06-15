#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../cp0_app_internal_utils.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef HAL_PLATFORM_SDL
#include "hal/hal_network.h"
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

int cp0_network_list(cp0_netif_info_t *entries, int max_entries, int *out_count)
{
    if (!out_count)
        return -1;
    *out_count = 0;

#ifdef HAL_PLATFORM_SDL
    if (!entries || max_entries <= 0)
        return hal_network_list(nullptr, 0, out_count);

    std::vector<hal_netif_info_t> hal_entries(static_cast<size_t>(max_entries));
    int ret = hal_network_list(hal_entries.data(), max_entries, out_count);
    int count = *out_count;
    if (count > max_entries)
        count = max_entries;

    for (int i = 0; i < count; ++i) {
        cp0_copy_string(entries[i].iface, sizeof(entries[i].iface), hal_entries[i].iface);
        cp0_copy_string(entries[i].ipv4, sizeof(entries[i].ipv4), hal_entries[i].ipv4);
        cp0_copy_string(entries[i].netmask, sizeof(entries[i].netmask), hal_entries[i].netmask);
        entries[i].is_up = hal_entries[i].is_up;
    }
    *out_count = count;
    return ret;
#else
    if (!entries || max_entries <= 0)
        return 0;

    struct ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) != 0)
        return -1;

    for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (std::strcmp(ifa->ifa_name, "lo") == 0)
            continue;
        if (*out_count >= max_entries)
            break;

        cp0_netif_info_t *e = &entries[*out_count];
        cp0_copy_string(e->iface, sizeof(e->iface), ifa->ifa_name);
        struct sockaddr_in *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
        inet_ntop(AF_INET, &sa->sin_addr, e->ipv4, sizeof(e->ipv4));
        if (ifa->ifa_netmask) {
            struct sockaddr_in *nm = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_netmask);
            inet_ntop(AF_INET, &nm->sin_addr, e->netmask, sizeof(e->netmask));
        } else {
            cp0_copy_string(e->netmask, sizeof(e->netmask), "N/A");
        }
        e->is_up = (ifa->ifa_flags & IFF_UP) ? 1 : 0;
        (*out_count)++;
    }
    freeifaddrs(ifap);
    return 0;
#endif
}

class WifiSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    WifiSystem()
    {
        update_status_cache();
        worker_ = std::thread([this]() { poll_loop(); });
        worker_.detach();
    }

    void api_call(arg_t arg, callback_t callback)
    {
        const std::string cmd = arg.empty() ? "" : arg.front();
        if (cmd == "Status") {
            cp0_wifi_status_t st = get_status();
            report(callback, 0, encode_status(st));
        } else if (cmd == "Scan") {
            int max_count = arg.size() >= 2 ? std::atoi(second_arg(arg).c_str()) : CP0_WIFI_AP_MAX;
            std::vector<cp0_wifi_ap_t> aps(std::max(0, max_count));
            int count = scan(aps.empty() ? nullptr : aps.data(), static_cast<int>(aps.size()));
            report(callback, count, encode_scan(aps.data(), count));
        } else if (cmd == "Connect") {
            const std::string ssid = nth_arg(arg, 1);
            const std::string password = nth_arg(arg, 2);
            report(callback, connect(ssid.c_str(), password.empty() ? nullptr : password.c_str()), "");
        } else if (cmd == "Disconnect") {
            report(callback, disconnect(), "");
        } else if (cmd == "ProfileForget") {
            const std::string ssid = nth_arg(arg, 1);
            report(callback, profile_forget(ssid.c_str()), "");
        } else if (cmd == "ProfileExists") {
            const std::string ssid = nth_arg(arg, 1);
            report(callback, profile_exists(ssid.c_str()), "");
        } else if (cmd == "ProfileDisconnectActive") {
            report(callback, profile_disconnect_active(), "");
        } else {
            report(callback, -1, "unknown wifi api command");
        }
    }

    cp0_wifi_status_t get_status()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_;
    }

    int scan(cp0_wifi_ap_t *out, int max_aps)
    {
        const char *rescan_argv[] = {"nmcli", "dev", "wifi", "rescan", nullptr};
        cp0_process_run_argv(rescan_argv, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        char output[8192] = {};
        const char *scan_argv[] = {"nmcli", "-t", "-f", "SSID,SIGNAL,SECURITY,IN-USE", "dev", "wifi", "list", nullptr};
        if (cp0_process_capture_argv(scan_argv, output, sizeof(output)) != 0)
            return 0;

        std::vector<cp0_wifi_ap_t> aps;
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;

            cp0_wifi_ap_t ap{};
            if (!parse_scan_line(line, ap))
                continue;
            upsert_ap(aps, ap);
        }

        const int count = static_cast<int>(aps.size());
        if (out && max_aps > 0) {
            const int copy_count = std::min(count, max_aps);
            for (int i = 0; i < copy_count; ++i)
                out[i] = aps[static_cast<size_t>(i)];
            return copy_count;
        }
        return count;
    }

    int connect(const char *ssid, const char *password)
    {
        if (!ssid || !ssid[0])
            return -1;

        char output[4096] = {};
        int ret = -1;
        if (password && password[0]) {
            const char *argv[] = {"nmcli", "dev", "wifi", "connect", ssid, "password", password, nullptr};
            ret = cp0_process_capture_argv(argv, output, sizeof(output));
        } else {
            const char *argv[] = {"nmcli", "con", "up", "id", ssid, nullptr};
            ret = cp0_process_capture_argv(argv, output, sizeof(output));
        }

        if (ret == 0 || std::string(output).find("successfully") != std::string::npos) {
            update_status_cache();
            return 0;
        }
        return -1;
    }

    int disconnect()
    {
        int ret = profile_disconnect_active();
        update_status_cache();
        return ret;
    }

    int profile_forget(const char *ssid)
    {
        if (!ssid || !ssid[0])
            return -1;
        const char *argv[] = {"nmcli", "con", "delete", "id", ssid, nullptr};
        return cp0_process_run_argv(argv, 0);
    }

    int profile_exists(const char *ssid)
    {
        if (!ssid || !ssid[0])
            return 0;
        char output[4096] = {};
        const char *argv[] = {"nmcli", "-t", "-f", "NAME", "con", "show", nullptr};
        if (cp0_process_capture_argv(argv, output, sizeof(output)) != 0)
            return 0;
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line == ssid)
                return 1;
        }
        return 0;
    }

    int profile_disconnect_active()
    {
        const std::string active = active_connection_name();
        if (active.empty())
            return -1;
        const char *argv[] = {"nmcli", "con", "down", "id", active.c_str(), nullptr};
        return cp0_process_run_argv(argv, 0);
    }

private:
    std::mutex mutex_;
    cp0_wifi_status_t cache_{};
    std::thread worker_;
    std::atomic<bool> running_{true};

    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        for (size_t i = 0; i < index && it != arg.end(); ++i)
            ++it;
        return it == arg.end() ? std::string() : *it;
    }

    static std::string second_arg(const arg_t &arg)
    {
        return nth_arg(arg, 1);
    }

    static std::vector<std::string> split_colon(const std::string &line)
    {
        std::vector<std::string> cols;
        std::string current;
        for (char ch : line) {
            if (ch == ':') {
                cols.push_back(current);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
        cols.push_back(current);
        return cols;
    }

    static bool parse_scan_line(const std::string &line, cp0_wifi_ap_t &ap)
    {
        auto cols = split_colon(line);
        if (cols.size() < 4 || cols[0].empty())
            return false;
        cp0_copy_string(ap.ssid, sizeof(ap.ssid), cols[0]);
        ap.signal = std::atoi(cols[1].c_str());
        cp0_copy_string(ap.security, sizeof(ap.security), cols[2]);
        ap.in_use = cols[3].find('*') != std::string::npos ? 1 : 0;
        return true;
    }

    static void upsert_ap(std::vector<cp0_wifi_ap_t> &aps, const cp0_wifi_ap_t &ap)
    {
        auto it = std::find_if(aps.begin(), aps.end(), [&](const cp0_wifi_ap_t &existing) {
            return std::strcmp(existing.ssid, ap.ssid) == 0;
        });
        if (it == aps.end()) {
            aps.push_back(ap);
            return;
        }

        int in_use = it->in_use || ap.in_use;
        if (ap.signal > it->signal)
            *it = ap;
        it->in_use = in_use;
    }

    static std::string encode_status(const cp0_wifi_status_t &st)
    {
        std::ostringstream oss;
        oss << st.connected << ':' << st.ssid << ':' << st.ip << ':' << st.signal;
        return oss.str();
    }

    static std::string encode_scan(const cp0_wifi_ap_t *aps, int count)
    {
        std::ostringstream oss;
        for (int i = 0; aps && i < count; ++i) {
            oss << aps[i].ssid << ':' << aps[i].signal << ':' << aps[i].security << ':' << aps[i].in_use << '\n';
        }
        return oss.str();
    }

    void poll_loop()
    {
        while (running_.load()) {
            update_status_cache();
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    void update_status_cache()
    {
        cp0_wifi_status_t st{};
        read_status(st);
        std::lock_guard<std::mutex> lock(mutex_);
        cache_ = st;
    }

    static void read_status(cp0_wifi_status_t &st)
    {
        char output[4096] = {};
        const char *status_argv[] = {"nmcli", "-t", "-f", "TYPE,CONNECTION", "dev", "status", nullptr};
        if (cp0_process_capture_argv(status_argv, output, sizeof(output)) == 0) {
            std::istringstream lines(output);
            std::string line;
            while (std::getline(lines, line)) {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.rfind("wifi:", 0) != 0)
                    continue;
                std::string name = line.substr(5);
                if (!name.empty() && name != "--") {
                    st.connected = 1;
                    cp0_copy_string(st.ssid, sizeof(st.ssid), name);
                }
                break;
            }
        }

        if (!st.connected)
            return;

        const char *signal_argv[] = {"nmcli", "-t", "-f", "IN-USE,SIGNAL", "dev", "wifi", "list", "--rescan", "no", nullptr};
        if (cp0_process_capture_argv(signal_argv, output, sizeof(output)) == 0) {
            std::istringstream lines(output);
            std::string line;
            while (std::getline(lines, line)) {
                if (line.rfind("*:", 0) == 0) {
                    st.signal = std::atoi(line.c_str() + 2);
                    break;
                }
            }
        }

        const char *ip_argv[] = {"ip", "-4", "-o", "addr", "show", "wlan0", nullptr};
        if (cp0_process_capture_argv(ip_argv, output, sizeof(output)) == 0) {
            std::string line(output);
            auto pos = line.find("inet ");
            if (pos != std::string::npos) {
                std::string ip = line.substr(pos + 5);
                auto slash = ip.find('/');
                if (slash != std::string::npos)
                    ip.resize(slash);
                cp0_copy_string(st.ip, sizeof(st.ip), ip);
            }
        }
    }

    static std::string active_connection_name()
    {
        char output[4096] = {};
        const char *argv[] = {"nmcli", "-t", "-f", "NAME", "con", "show", "--active", nullptr};
        if (cp0_process_capture_argv(argv, output, sizeof(output)) != 0)
            return {};
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty() && line != "lo")
                return line;
        }
        return {};
    }
};

extern "C" void init_wifi(void)
{
    auto wifi = std::make_shared<WifiSystem>();
    cp0_signal_wifi_api.append([wifi](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        wifi->api_call(std::move(arg), std::move(callback));
    });
}
