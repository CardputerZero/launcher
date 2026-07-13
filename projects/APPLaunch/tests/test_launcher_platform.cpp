#include "launcher_platform.hpp"

#include <cassert>
#include <string>

int main()
{
    std::string decoded;
    assert(launcher_platform::decode_field("normal.desktop", decoded));
    assert(decoded == "normal.desktop");
    assert(launcher_platform::decode_field("tab%09line%0Apercent%25.desktop", decoded));
    assert(decoded == "tab\tline\npercent%.desktop");
    assert(!launcher_platform::decode_field("bad%", decoded));
    assert(!launcher_platform::decode_field("bad%XZ", decoded));
    return 0;
}
