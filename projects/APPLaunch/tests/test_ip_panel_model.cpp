#include "../main/ui/model/ip_panel_model.hpp"

#include <cassert>

namespace {

IpInterfaceInfo interface(const char *name)
{
    return {name, "192.0.2.1", "255.255.255.0", true};
}

} // namespace

int main()
{
    static_assert(noexcept(ip_panel_refresh_callback_allowed(true, true)));
    static_assert(ip_panel_refresh_callback_allowed(true, true));
    static_assert(!ip_panel_refresh_callback_allowed(false, true));
    static_assert(!ip_panel_refresh_callback_allowed(true, false));

    int root = 0;
    int stale_root = 0;
    static_assert(noexcept(ip_panel_root_callback_allowed(&root, &root)));
    assert(ip_panel_root_callback_allowed(&root, &root));
    assert(!ip_panel_root_callback_allowed(&stale_root, &root));
    assert(!ip_panel_root_callback_allowed(static_cast<int *>(nullptr), &root));

    int background = 1;
    int list = 2;
    static_assert(noexcept(ip_panel_view_ready(&background, &list)));
    assert(ip_panel_view_ready(&background, &list));
    assert(!ip_panel_view_ready(static_cast<int *>(nullptr), &list));
    assert(!ip_panel_view_ready(&background, static_cast<int *>(nullptr)));

    static_assert(noexcept(ip_panel_owned_delete_callback_allowed(
        &background, &background)));
    assert(ip_panel_owned_delete_callback_allowed(&background, &background));
    assert(!ip_panel_owned_delete_callback_allowed(&background, &list));
    assert(!ip_panel_owned_delete_callback_allowed(
        static_cast<int *>(nullptr), &background));

    IpPanelModel model;
    model.replace({interface("wlan0"), interface("eth0"), interface("lo"),
                   interface("usb0"), interface("docker0"), interface("br0")});
    assert(model.interfaces().size() == 6);
    assert(model.interfaces()[0].name == "br0");
    assert(model.interfaces()[5].name == "wlan0");
    assert(model.selected_index() == 0);
    assert(!model.select_previous());

    assert(model.select_next());
    assert(model.select_next());
    assert(model.select_next());
    assert(model.interfaces()[model.selected_index()].name == "lo");
    assert(model.visible_offset(4) == 1);

    model.replace({interface("new0"), interface("lo"), interface("eth0"),
                   interface("wlan0"), interface("br0")});
    assert(model.interfaces()[model.selected_index()].name == "lo");
    assert(model.visible_offset(4) == 0);

    model.replace({interface("eth0")});
    assert(model.selected_index() == 0);
    assert(!model.select_next());

    model.replace({});
    assert(model.selected_index() == 0);
    assert(model.visible_offset(4) == 0);

    model.replace({interface("eth0"), interface("wlan0")});
    assert(model.select_next());
    assert(!model.apply_refresh(false, {interface("failed0")}));
    assert(model.interfaces().size() == 2);
    assert(model.interfaces()[model.selected_index()].name == "wlan0");
    assert(model.apply_refresh(true, {interface("usb0"), interface("wlan0")}));
    assert(model.interfaces()[model.selected_index()].name == "wlan0");
}
