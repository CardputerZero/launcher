#include "../main/ui/page_app/setting/adb_state.hpp"
#include <cassert>

int main()
{
    using namespace setting;
    AdbStatus active = parse_adb_status("peripheral=yes\nadbd=active\nenabled=enabled\n");
    assert(active.valid && active.active && active.enabled);
    AdbStatus pending = parse_adb_status("peripheral=no\nadbd=inactive\nenabled=enabled\n");
    assert(pending.valid && !pending.active && pending.enabled);
    AdbStatus off = parse_adb_status("peripheral=no\nadbd=inactive\nenabled=disabled\n");
    assert(off.valid && !off.active && !off.enabled);
    assert(!parse_adb_status("unrelated=active\n").valid);
    assert(!parse_adb_status(nullptr).valid);
    assert(adb_toggle_succeeded(0, 0));
    assert(adb_toggle_succeeded(2, 10));
    assert(!adb_toggle_succeeded(2, 1));
    assert(adb_reboot_required(2, 10));
    assert(!adb_reboot_required(0, 0));
}
