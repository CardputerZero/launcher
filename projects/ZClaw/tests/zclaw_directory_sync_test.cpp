#include "zclaw_directory_sync.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

int main()
{
    char directory[] = "/tmp/zclaw-directory-sync-test-XXXXXX";
    const char *root = ::mkdtemp(directory);
    assert(root);
    const std::filesystem::path base(root);
    const std::filesystem::path file = base / "config.tsv";
    std::ofstream(file) << "data";

    std::string error = "stale";
    assert(zclaw::sync_parent_directory(file.string(), &error));
    assert(error.empty());

    error.clear();
    assert(!zclaw::sync_parent_directory(
        (base / "missing" / "config.tsv").string(), &error));
    assert(error.rfind("Could not sync parent directory: ", 0) == 0);

    error.clear();
    assert(!zclaw::sync_parent_directory(
        (file / "not-a-child").string(), &error));
    assert(!error.empty());

    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);
    return 0;
}
