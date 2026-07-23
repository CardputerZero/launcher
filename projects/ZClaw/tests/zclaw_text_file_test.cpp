#include "zclaw_text_file.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        char path[] = "/tmp/zclaw-text-file-test-XXXXXX";
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

}  // namespace

int main()
{
    TemporaryDirectory temporary;
    zclaw::TextFileReadResult result =
        zclaw::read_text_file((temporary.path() / "missing").string());
    assert(result.status == zclaw::TextFileReadStatus::NotFound);
    assert(result.contents.empty());
    assert(result.error.empty());

    const fs::path empty = temporary.path() / "empty";
    std::ofstream(empty).close();
    result = zclaw::read_text_file(empty.string());
    assert(result.status == zclaw::TextFileReadStatus::Loaded);
    assert(result.contents.empty());
    assert(result.error.empty());

    const fs::path content = temporary.path() / "content";
    const char raw_contents[] = "first\nsecond\0tail";
    const std::string expected(raw_contents, sizeof(raw_contents) - 1);
    std::ofstream output(content, std::ios::binary);
    output.write(expected.data(), static_cast<std::streamsize>(expected.size()));
    output.close();
    result = zclaw::read_text_file(content.string());
    assert(result.status == zclaw::TextFileReadStatus::Loaded);
    assert(result.contents == expected);

    result = zclaw::read_text_file(temporary.path().string());
    assert(result.status == zclaw::TextFileReadStatus::Error);
    assert(result.contents.empty());
    assert(!result.error.empty());
    return 0;
}
