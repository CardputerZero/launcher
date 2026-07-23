#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FileBrowserEntry
{
    std::string name;
    bool is_directory = false;
    std::int64_t size = 0;
};

class FileBrowserModel
{
public:
    static constexpr std::size_t MAX_ENTRIES = 4096;

    const std::string &current_path() const { return current_path_; }
    const std::vector<FileBrowserEntry> &entries() const { return entries_; }
    int selected_index() const { return selected_index_; }
    bool listing_failed() const { return listing_failed_; }

    bool apply_listing(int result_code, const std::string &data);
    bool select_previous();
    bool select_next();
    bool enter_selected();
    bool navigate_parent();

private:
    static bool decode_field(const std::string &encoded, std::string &decoded);
    static std::string parent_path(const std::string &path);
    static std::string join_path(const std::string &base, const std::string &name);

    std::string current_path_ = "/";
    std::vector<FileBrowserEntry> entries_;
    int selected_index_ = 0;
    bool listing_failed_ = false;
};
