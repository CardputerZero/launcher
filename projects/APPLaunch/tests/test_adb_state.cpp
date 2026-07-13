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
    assert(adb_toggle_succeeded(CP0_SUDO_RESULT_SUCCESS, 0));
    assert(adb_toggle_succeeded(CP0_SUDO_RESULT_EXEC_FAILED, 10));
    assert(!adb_toggle_succeeded(CP0_SUDO_RESULT_EXEC_FAILED, 1));
    assert(!adb_toggle_succeeded(CP0_SUDO_RESULT_AUTH_FAILED, 10));
    assert(adb_reboot_required(CP0_SUDO_RESULT_EXEC_FAILED, 10));
    assert(!adb_reboot_required(CP0_SUDO_RESULT_SUCCESS, 0));
    assert(adb_state_after_failure(active, false));
    assert(adb_state_after_failure(pending, false));
    assert(!adb_state_after_failure(off, true));
    assert(adb_state_after_failure(AdbStatus{}, true));
    assert(!adb_state_after_failure(AdbStatus{}, false));
}
