#include "../main/ui/model/system_page_model.hpp"

#include <cassert>
#include <string>

int main()
{
    using namespace system_page;

    std::string current = "stable";
    assert(!commit_if_success(false, std::string("partial"), current));
    assert(current == "stable");
    assert(commit_if_success(true, std::string("committed"), current));
    assert(current == "committed");

    NetworkInfo network = parse_network_info("192.0.2.5\n192.0.2.1\naa:bb:cc:dd:ee:ff\n");
    assert(network.ip == "192.0.2.5");
    assert(network.gateway == "192.0.2.1");
    assert(network.mac == "aa:bb:cc:dd:ee:ff");

    network = parse_network_info("10.0.0.2\r\n\r\n");
    assert(network.ip == "10.0.0.2");
    assert(network.gateway == "--");
    assert(network.mac == "--");

    AccountInfo account = parse_account_info("alice\r\ncardputer\r\n");
    assert(account.username == "alice");
    assert(account.hostname == "cardputer");

    account = parse_account_info("");
    assert(account.username == "--");
    assert(account.hostname == "--");

    assert(version_label("abc123") == "Version: abc123");
    assert(version_label("") == "Version: --");
    assert(std::string(update_request(UpdateAction::CheckSystem)) == "AptUpdateBackground");
    assert(std::string(update_request(UpdateAction::UpdateLauncher)) ==
           "UpdateLauncherBackground");

    auto toggle = extport_toggle_outcome(false, true, true, true, true);
    assert(toggle.committed && toggle.value);
    assert(!toggle.restore_gpio && !toggle.restore_config);

    toggle = extport_toggle_outcome(true, false, false, false, false);
    assert(!toggle.committed && toggle.value);
    assert(!toggle.restore_gpio && !toggle.restore_config);

    toggle = extport_toggle_outcome(false, true, true, false, false);
    assert(!toggle.committed && !toggle.value);
    assert(toggle.restore_gpio && !toggle.restore_config);

    toggle = extport_toggle_outcome(true, false, true, true, false);
    assert(!toggle.committed && toggle.value);
    assert(toggle.restore_gpio && toggle.restore_config);
}
