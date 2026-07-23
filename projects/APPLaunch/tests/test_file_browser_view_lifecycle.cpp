#include "../main/ui/model/file_browser_view_lifecycle.hpp"

#include <cassert>

int main()
{
    int delete_target = 0;
    int bubbled_target = 0;
    static_assert(noexcept(file_browser_owned_delete_callback_allowed(
        &delete_target, &delete_target)));
    assert(file_browser_owned_delete_callback_allowed(
        &delete_target, &delete_target));
    assert(!file_browser_owned_delete_callback_allowed(
        &delete_target, &bubbled_target));
    assert(!file_browser_owned_delete_callback_allowed(
        static_cast<int *>(nullptr), &delete_target));

    FileBrowserViewLifecycle view;
    int background = 0;
    int path = 0;
    int first_list = 0;
    int second_list = 0;

    assert(!view.commit_tree(&background, &path, nullptr));
    assert(!view.ready() && !view.background());
    assert(view.commit_tree(&background, &path, &first_list));
    assert(view.ready());

    assert(!view.replace_list(nullptr, &second_list));
    assert(view.replace_list(&first_list, &second_list));
    view.erased(&first_list);
    assert(view.ready() && view.list_container() == &second_list);

    view.erased(&path);
    assert(!view.ready() && !view.path_label());
    view.erased(&background);
    assert(!view.background() && !view.list_container());

    assert(view.commit_tree(&background, &path, &first_list));
    view.clear(); // Constructor failure detaches callbacks before base-tree teardown.
    assert(!view.ready());
    assert(!view.background() && !view.path_label() && !view.list_container());
}
