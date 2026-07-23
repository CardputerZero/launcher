#include "cp0_osinfo_contract.hpp"

#include "cp0_osinfo_codec.hpp"

#include <iterator>

namespace cp0::osinfo {
namespace {

constexpr int kMaxNetworkEntries = 64;

int decimal_field(const std::string &value, size_t offset, size_t length)
{
    int result = 0;
    for (size_t index = offset; index < offset + length; ++index) {
        if (value[index] < '0' || value[index] > '9')
            return -1;
        result = result * 10 + value[index] - '0';
    }
    return result;
}

bool valid_timestamp(const std::string &value)
{
    if (value.size() != 19 || value[4] != '-' || value[7] != '-' || value[10] != ' ' ||
        value[13] != ':' || value[16] != ':')
        return false;
    const int year = decimal_field(value, 0, 4);
    const int month = decimal_field(value, 5, 2);
    const int day = decimal_field(value, 8, 2);
    const int hour = decimal_field(value, 11, 2);
    const int minute = decimal_field(value, 14, 2);
    const int second = decimal_field(value, 17, 2);
    if (year < 0 || month < 1 || month > 12 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 59)
        return false;
    constexpr int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int days = days_per_month[month - 1];
    if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
        ++days;
    return day >= 1 && day <= days;
}

std::string argument_at(const std::list<std::string> &arguments, size_t index)
{
    auto iterator = arguments.begin();
    for (size_t current = 0; current < index && iterator != arguments.end(); ++current)
        ++iterator;
    return iterator == arguments.end() ? std::string() : *iterator;
}

} // namespace

Result dispatch(const std::list<std::string> &arguments, const Operations &operations)
{
    try {
    const std::string command = argument_at(arguments, 0);
    if (command == "NetworkDefaultInfoRead" || command == "EthInfoRead") {
        cp0_eth_info_t info{};
        if (!operations.read_eth_info)
            return {};
        const int code = operations.read_eth_info(command == "NetworkDefaultInfoRead", &info);
        return {code, encode_eth_info(info)};
    }
    if (command == "NetworkList") {
        if (!operations.list_networks)
            return {};
        cp0_netif_info_t entries[kMaxNetworkEntries]{};
        int count = 0;
        const int code = operations.list_networks(entries, kMaxNetworkEntries, &count);
        if (code != 0)
            return {code, {}};
        if (count < 0 || count > kMaxNetworkEntries)
            return {-1, {}};
        return {0, encode_network_list(entries, count)};
    }
    if (command == "AccountInfoRead") {
        cp0_account_info_t info{};
        if (!operations.read_account_info)
            return {};
        const int code = operations.read_account_info(&info);
        return {code, encode_account_info(info)};
    }
    if (command == "TimeSet") {
        const std::string timestamp = argument_at(arguments, 1);
        if (!valid_timestamp(timestamp))
            return {-1, {}};
        return operations.set_time ? Result{operations.set_time(timestamp), {}} : Result{};
    }
    if (command == "LocalTime") {
        if (!operations.read_local_time)
            return {};
        std::tm value{};
        const int code = operations.read_local_time(&value);
        return {code, code == 0 ? encode_local_time(value) : std::string()};
    }
    if (command == "RandomU32") {
        return operations.random_u32 ? Result{0, std::to_string(operations.random_u32())} : Result{};
    }
    if (command == "NtpGet") {
        return operations.get_ntp ? Result{operations.get_ntp(), {}} : Result{};
    }
    if (command == "NtpSet") {
        const std::string enabled = argument_at(arguments, 1);
        if (enabled != "0" && enabled != "1")
            return {-1, {}};
        return operations.set_ntp ? Result{operations.set_ntp(enabled == "1"), {}} : Result{};
    }
    if (command == "AptUpdateBackground") {
        return operations.apt_update_background ? Result{operations.apt_update_background(), {}} : Result{};
    }
    if (command == "UpdateLauncherBackground") {
        return operations.update_launcher_background
                   ? Result{operations.update_launcher_background(), {}}
                   : Result{};
    }
        return {-1, "unknown osinfo api command"};
    } catch (...) {
        return {-1, "osinfo backend failure"};
    }
}

} // namespace cp0::osinfo
