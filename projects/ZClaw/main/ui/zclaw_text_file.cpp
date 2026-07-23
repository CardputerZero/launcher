#include "zclaw_text_file.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace zclaw {

TextFileReadResult read_text_file(const std::string &path)
{
    TextFileReadResult result;
    std::error_code status_error;
    const std::filesystem::file_status status =
        std::filesystem::status(path, status_error);
    if (status_error) {
        result.status = status_error == std::errc::no_such_file_or_directory
                            ? TextFileReadStatus::NotFound
                            : TextFileReadStatus::Error;
        if (result.status == TextFileReadStatus::Error)
            result.error = "Could not inspect file: " + status_error.message();
        return result;
    }
    if (!std::filesystem::is_regular_file(status)) {
        result.error = "Could not open file: Not a regular file";
        return result;
    }

    errno = 0;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        const int open_error = errno;
        result.status = open_error == ENOENT ? TextFileReadStatus::NotFound
                                             : TextFileReadStatus::Error;
        if (result.status == TextFileReadStatus::Error) {
            result.error = "Could not open file: " +
                           std::string(std::strerror(open_error ? open_error : EIO));
        }
        return result;
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    if (file.bad()) {
        const int read_error = errno;
        result.error = "Could not read file: " +
                       std::string(std::strerror(read_error ? read_error : EIO));
        return result;
    }

    result.status = TextFileReadStatus::Loaded;
    result.contents = contents.str();
    return result;
}

}  // namespace zclaw
