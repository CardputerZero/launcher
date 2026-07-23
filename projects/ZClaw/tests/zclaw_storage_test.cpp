#include "zclaw_storage.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        char path[] = "/tmp/zclaw-storage-test-XXXXXX";
        const char *created = ::mkdtemp(path);
        assert(created);
        path_ = created;
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    const fs::path &path() const { return path_; }

private:
    fs::path path_;
};

mode_t mode(const fs::path &path)
{
    struct stat status {};
    assert(::stat(path.c_str(), &status) == 0);
    return status.st_mode & 0777;
}

}  // namespace

int main()
{
    assert(zclaw::ensure_private_parent_directory("settings.tsv"));
    assert(zclaw::ensure_private_parent_directory(""));
    assert(zclaw::ensure_private_parent_directory("/settings.tsv"));

    TemporaryDirectory temporary;
    const fs::path nested = temporary.path() / "one" / "two" / "settings.tsv";
    std::string error;
    assert(zclaw::ensure_private_parent_directory(nested.string(), &error));
    assert(fs::is_directory(temporary.path() / "one"));
    assert(fs::is_directory(temporary.path() / "one" / "two"));
    assert(mode(temporary.path() / "one") == 0700);
    assert(mode(temporary.path() / "one" / "two") == 0700);

    const fs::path wide = temporary.path() / "wide";
    assert(fs::create_directory(wide));
    assert(::chmod(wide.c_str(), 0777) == 0);
    assert(zclaw::ensure_private_parent_directory(
        (wide / "settings.tsv").string(), &error));
    assert(mode(wide) == 0700);

    const fs::path symlink_target = temporary.path() / "symlink-target";
    assert(fs::create_directory(symlink_target));
    assert(::chmod(symlink_target.c_str(), 0755) == 0);
    const fs::path symlink_parent = temporary.path() / "symlink-parent";
    assert(::symlink(symlink_target.c_str(), symlink_parent.c_str()) == 0);
    error.clear();
    assert(!zclaw::ensure_private_parent_directory(
        (symlink_parent / "settings.tsv").string(), &error));
    assert(!error.empty());
    assert(mode(symlink_target) == 0755);

    const fs::path nested_symlink = temporary.path() / "nested-symlink";
    assert(::symlink(symlink_target.c_str(), nested_symlink.c_str()) == 0);
    error.clear();
    assert(!zclaw::ensure_private_parent_directory(
        (nested_symlink / "new" / "settings.tsv").string(), &error));
    assert(!fs::exists(symlink_target / "new"));
    assert(mode(symlink_target) == 0755);

    const fs::path blocking_file = temporary.path() / "blocked";
    std::ofstream(blocking_file) << "not a directory";
    error.clear();
    assert(!zclaw::ensure_private_parent_directory(
        (blocking_file / "child" / "settings.tsv").string(), &error));
    assert(!error.empty());
    return 0;
}
