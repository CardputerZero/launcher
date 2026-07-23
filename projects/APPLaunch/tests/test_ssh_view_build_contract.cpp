#include "../main/ui/model/ssh_view_build_contract.hpp"

#include <cassert>

int main()
{
    int background = 0;
    int child = 0;
    static_assert(noexcept(ssh_owned_delete_callback_allowed(
        &background, &background)));
    assert(ssh_owned_delete_callback_allowed(&background, &background));
    assert(!ssh_owned_delete_callback_allowed(&child, &background));
    assert(!ssh_owned_delete_callback_allowed(
        static_cast<int *>(nullptr), &background));

    int root_screen = 0;
    int stale_root = 0;
    static_assert(noexcept(ssh_page_event_callback_allowed(
        &root_screen, &root_screen)));
    assert(ssh_page_event_callback_allowed(&root_screen, &root_screen));
    assert(!ssh_page_event_callback_allowed(&stale_root, &root_screen));
    assert(!ssh_page_event_callback_allowed(
        static_cast<int *>(nullptr), &root_screen));
    static_assert(noexcept(ssh_restore_completion_allowed(
        false, static_cast<int *>(nullptr))));
    assert(ssh_restore_completion_allowed(true, &root_screen));
    assert(!ssh_restore_completion_allowed(false, &root_screen));
    assert(!ssh_restore_completion_allowed(
        true, static_cast<int *>(nullptr)));

    SshViewBuildContract build(3);
    assert(!build.ready());
    build.row_completed();
    build.row_completed();
    build.hint_completed();
    assert(!build.ready());
    build.row_completed();
    assert(build.ready());

    SshViewBuildContract missing_hint(1);
    missing_hint.row_completed();
    assert(!missing_hint.ready());

    SshViewBuildContract too_many_rows(1);
    too_many_rows.row_completed();
    too_many_rows.hint_completed();
    assert(too_many_rows.ready());
    too_many_rows.row_completed();
    assert(!too_many_rows.ready());
}
