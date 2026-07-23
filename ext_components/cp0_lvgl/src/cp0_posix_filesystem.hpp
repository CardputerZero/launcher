#pragma once

#include "cp0_lvgl_app.h"

#include <cstddef>
#include <string>

namespace cp0_posix_filesystem {

int list_directory(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count,
                   bool errno_on_open_failure);
int encode_directory(const char *path, bool include_size, bool errno_on_open_failure,
                     std::string &output);
int read_file_limited(const std::string &path, size_t max_bytes, std::string &output);

} // namespace cp0_posix_filesystem
