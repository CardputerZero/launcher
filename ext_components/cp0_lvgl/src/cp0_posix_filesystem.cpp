#include "cp0_posix_filesystem.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <new>
#include <sstream>
#include <sys/stat.h>

namespace cp0_posix_filesystem {
namespace {

bool is_dot_entry(const char *name)
{
    return name && name[0] == '.' &&
           (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

std::string join_path(const char *base, const char *name)
{
    std::string path = base ? base : "";
    if (path != "/" && !path.empty() && path.back() != '/') path.push_back('/');
    path += name ? name : "";
    return path;
}

bool directory_entry_is_dir(const char *path, const dirent &entry)
{
    if (entry.d_type == DT_DIR) return true;
    if (entry.d_type != DT_UNKNOWN) return false;
    struct stat status{};
    return stat(join_path(path, entry.d_name).c_str(), &status) == 0 && S_ISDIR(status.st_mode);
}

std::string encode_field(const char *value)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char character : std::string(value ? value : "")) {
        if (character == '%' || character == '\t' || character == '\n' || character == '\r') {
            encoded.push_back('%');
            encoded.push_back(hex[character >> 4]);
            encoded.push_back(hex[character & 0x0f]);
        } else {
            encoded.push_back(static_cast<char>(character));
        }
    }
    return encoded;
}

int open_failure(bool errno_on_open_failure)
{
    return errno_on_open_failure ? -errno : -1;
}

} // namespace

int list_directory(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count,
                   bool errno_on_open_failure)
{
    if (out_count) *out_count = 0;
    if (!path || !out_count) return -1;
    if (!entries || max_entries <= 0) return 0;

    DIR *directory = opendir(path);
    if (!directory) return open_failure(errno_on_open_failure);
    while (dirent *entry = readdir(directory)) {
        if (is_dot_entry(entry->d_name)) continue;
        if (*out_count >= max_entries) break;
        cp0_dirent_t &output = entries[*out_count];
        std::strncpy(output.name, entry->d_name, sizeof(output.name) - 1);
        output.name[sizeof(output.name) - 1] = '\0';
        output.is_dir = directory_entry_is_dir(path, *entry) ? 1 : 0;
        ++(*out_count);
    }
    closedir(directory);
    return 0;
}

int encode_directory(const char *path, bool include_size, bool errno_on_open_failure,
                     std::string &output)
{
    DIR *directory = opendir(path);
    if (!directory) return open_failure(errno_on_open_failure);
    try {
        std::ostringstream encoded;
        while (dirent *entry = readdir(directory)) {
            if (is_dot_entry(entry->d_name)) continue;
            const std::string full_path = join_path(path, entry->d_name);
            struct stat status{};
            if (include_size) {
                if (stat(full_path.c_str(), &status) != 0) continue;
            } else {
                status.st_mode = directory_entry_is_dir(path, *entry) ? S_IFDIR : S_IFREG;
            }
            encoded << (S_ISDIR(status.st_mode) ? 'D' : 'F') << '\t';
            if (include_size) encoded << static_cast<long long>(status.st_size) << '\t';
            encoded << encode_field(entry->d_name) << '\n';
        }
        output = encoded.str();
    } catch (const std::bad_alloc &) {
        closedir(directory);
        return -ENOMEM;
    }
    closedir(directory);
    return 0;
}

int read_file_limited(const std::string &path, size_t max_bytes, std::string &output)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return errno ? -errno : -EIO;
    const std::streamoff length = file.tellg();
    if (length < 0) return -EIO;
    if (static_cast<uintmax_t>(length) > max_bytes) return -EFBIG;
    try {
        output.resize(static_cast<size_t>(length));
    } catch (const std::bad_alloc &) {
        return -ENOMEM;
    }
    file.seekg(0);
    if (length > 0 && !file.read(&output[0], length)) {
        output.clear();
        return -EIO;
    }
    return 0;
}

} // namespace cp0_posix_filesystem
