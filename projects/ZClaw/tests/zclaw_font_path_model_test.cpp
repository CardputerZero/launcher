#include "zclaw_font_path_model.h"

#include <cassert>
#include <string>
#include <vector>

int main()
{
    const std::vector<std::string> existing = {"override.ttf", "second.ttf"};
    const auto path_exists = [&existing](const std::string &path) {
        for (const std::string &item : existing) {
            if (item == path)
                return true;
        }
        return false;
    };

    const std::vector<std::string> candidates = {
        "missing.ttf", "second.ttf", "later.ttf"
    };
    assert(zclaw::select_font_path("override.ttf", candidates, path_exists) ==
           "override.ttf");
    assert(zclaw::select_font_path("missing-override.ttf", candidates,
                                   path_exists) == "second.ttf");
    assert(zclaw::select_font_path("", candidates, path_exists) ==
           "second.ttf");
    assert(zclaw::select_font_path("", {"", "missing.ttf"}, path_exists)
               .empty());
    assert(zclaw::select_font_path("override.ttf", candidates, {}).empty());
    return 0;
}
