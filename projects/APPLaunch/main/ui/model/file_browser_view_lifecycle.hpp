#pragma once

template <typename Target, typename CurrentTarget>
constexpr bool file_browser_owned_delete_callback_allowed(
    Target target, CurrentTarget current_target) noexcept
{
    return target && target == current_target;
}

class FileBrowserViewLifecycle
{
public:
    using Handle = void *;

    bool commit_tree(Handle background, Handle path_label, Handle list_container)
    {
        if (!background || !path_label || !list_container) {
            clear();
            return false;
        }
        background_ = background;
        path_label_ = path_label;
        list_container_ = list_container;
        return true;
    }

    bool replace_list(Handle expected, Handle replacement)
    {
        if (!background_ || list_container_ != expected || !replacement) return false;
        list_container_ = replacement;
        return true;
    }

    void erased(Handle handle) noexcept
    {
        if (!handle) return;
        if (handle == background_) {
            clear();
            return;
        }
        if (handle == path_label_) path_label_ = nullptr;
        if (handle == list_container_) list_container_ = nullptr;
    }

    void clear()
    {
        background_ = nullptr;
        path_label_ = nullptr;
        list_container_ = nullptr;
    }

    bool ready() const { return background_ && path_label_ && list_container_; }
    Handle background() const { return background_; }
    Handle path_label() const { return path_label_; }
    Handle list_container() const { return list_container_; }

private:
    Handle background_ = nullptr;
    Handle path_label_ = nullptr;
    Handle list_container_ = nullptr;
};
