#include "zclaw_atomic_file.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main()
{
    char directory_template[] = "/tmp/zclaw-atomic-file-XXXXXX";
    const char *directory = ::mkdtemp(directory_template);
    assert(directory);
    const std::string path = std::string(directory) + "/config.tsv";

    std::string error = "stale";
    assert(zclaw::atomic_write_file(path, "first", {}, &error));
    assert(error.empty());
    struct stat state {};
    assert(::stat(path.c_str(), &state) == 0);
    assert((state.st_mode & 0777) == 0600);

    assert(zclaw::atomic_write_file(path, "second\nline", {0640}, &error));
    assert(::stat(path.c_str(), &state) == 0);
    assert((state.st_mode & 0777) == 0640);
    std::ifstream input(path, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    assert(contents == "second\nline");

    assert(!zclaw::atomic_write_file(
        std::string(directory) + "/missing/config.tsv", "data", {}, &error));
    assert(!error.empty());
    assert(::unlink(path.c_str()) == 0);
    assert(::rmdir(directory) == 0);
    return 0;
}
