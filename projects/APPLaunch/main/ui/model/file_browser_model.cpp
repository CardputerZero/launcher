#include "file_browser_model.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace {

int hex_value(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    return -1;
}

} // namespace

bool FileBrowserModel::decode_field(const std::string &encoded, std::string &decoded)
{
    decoded.clear();
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        if (encoded[index] != '%') {
            decoded.push_back(encoded[index]);
            continue;
        }
        if (index + 2 >= encoded.size()) return false;
        const int high = hex_value(encoded[index + 1]);
        const int low = hex_value(encoded[index + 2]);
        if (high < 0 || low < 0) return false;
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return decoded.find('\0') == std::string::npos &&
           decoded.find('/') == std::string::npos &&
           decoded.find('\\') == std::string::npos;
}

std::string FileBrowserModel::parent_path(const std::string &path)
{
    if (path.empty() || path == "/") return "/";
    const std::size_t end = path.find_last_not_of('/');
    if (end == std::string::npos) return "/";
    const std::size_t separator = path.rfind('/', end);
    return separator == 0 || separator == std::string::npos
               ? "/"
               : path.substr(0, separator);
}

std::string FileBrowserModel::join_path(const std::string &base,
                                        const std::string &name)
{
    if (name.empty()) return base.empty() ? "/" : base;
    return base.empty() || base == "/" ? "/" + name : base + "/" + name;
}

bool FileBrowserModel::apply_listing(int result_code, const std::string &data)
{
    entries_.clear();
    selected_index_ = 0;
    listing_failed_ = result_code != 0;
    if (listing_failed_) return false;

    std::istringstream stream(data);
    std::string line;
    while (entries_.size() < MAX_ENTRIES && std::getline(stream, line)) {
        const std::size_t first = line.find('\t');
        const std::size_t second = first == std::string::npos
                                       ? first
                                       : line.find('\t', first + 1);
        if (first != 1 || second == std::string::npos ||
            (line[0] != 'D' && line[0] != 'F'))
            continue;

        char *end = nullptr;
        errno = 0;
        const long long parsed = std::strtoll(line.c_str() + first + 1, &end, 10);
        if (errno == ERANGE || end != line.c_str() + second || parsed < 0) continue;

        FileBrowserEntry entry;
        entry.is_directory = line[0] == 'D';
        entry.size = entry.is_directory ? 0 : parsed;
        if (!decode_field(line.substr(second + 1), entry.name) || entry.name.empty() ||
            entry.name == "." || entry.name == "..")
            continue;
        entries_.push_back(std::move(entry));
    }

    std::sort(entries_.begin(), entries_.end(),
              [](const FileBrowserEntry &left, const FileBrowserEntry &right) {
                  if (left.is_directory != right.is_directory)
                      return left.is_directory > right.is_directory;
                  return left.name < right.name;
              });
    return true;
}

bool FileBrowserModel::select_previous()
{
    if (selected_index_ <= 0) return false;
    --selected_index_;
    return true;
}

bool FileBrowserModel::select_next()
{
    if (selected_index_ + 1 >= static_cast<int>(entries_.size())) return false;
    ++selected_index_;
    return true;
}

bool FileBrowserModel::enter_selected()
{
    if (selected_index_ < 0 || selected_index_ >= static_cast<int>(entries_.size()) ||
        !entries_[static_cast<std::size_t>(selected_index_)].is_directory)
        return false;
    current_path_ = join_path(
        current_path_, entries_[static_cast<std::size_t>(selected_index_)].name);
    entries_.clear();
    selected_index_ = 0;
    return true;
}

bool FileBrowserModel::navigate_parent()
{
    if (current_path_ == "/") return false;
    current_path_ = parent_path(current_path_);
    entries_.clear();
    selected_index_ = 0;
    return true;
}
