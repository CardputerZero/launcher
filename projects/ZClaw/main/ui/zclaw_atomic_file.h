#pragma once

#include <cstdint>
#include <string>

namespace zclaw {

struct AtomicFileOptions {
    std::uint32_t permissions = 0600;
};

bool atomic_write_file(const std::string &path, const std::string &contents,
                       const AtomicFileOptions &options = {},
                       std::string *error = nullptr);

}  // namespace zclaw
