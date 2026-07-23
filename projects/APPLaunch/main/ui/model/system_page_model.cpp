#include "system_page_model.hpp"

#include <sstream>

namespace system_page {
namespace {

std::string next_field(std::istringstream &lines)
{
    std::string value;
    if (!std::getline(lines, value))
        return "--";
    if (!value.empty() && value.back() == '\r')
        value.pop_back();
    return value.empty() ? "--" : value;
}

} // namespace

NetworkInfo parse_network_info(const std::string &payload)
{
    std::istringstream lines(payload);
    return {next_field(lines), next_field(lines), next_field(lines)};
}

AccountInfo parse_account_info(const std::string &payload)
{
    std::istringstream lines(payload);
    return {next_field(lines), next_field(lines)};
}

std::string version_label(const std::string &commit)
{
    return "Version: " + (commit.empty() ? std::string("--") : commit);
}

const char *update_request(UpdateAction action)
{
    switch (action) {
    case UpdateAction::CheckSystem:
        return "AptUpdateBackground";
    case UpdateAction::UpdateLauncher:
        return "UpdateLauncherBackground";
    }
    return "";
}

bool extport_toggle_value(bool previous, bool desired, bool gpio_succeeded)
{
    return gpio_succeeded ? desired : previous;
}

} // namespace system_page
