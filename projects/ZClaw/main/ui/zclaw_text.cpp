#include "zclaw_text.h"

namespace zclaw {

std::string trim_ascii_whitespace(const std::string &value)
{
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

}  // namespace zclaw
