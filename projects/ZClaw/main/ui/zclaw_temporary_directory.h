#pragma once

#include <memory>
#include <string>

namespace zclaw {

class TemporaryDirectory {
public:
    ~TemporaryDirectory();

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    static std::unique_ptr<TemporaryDirectory> create(
        const std::string &parent, const std::string &prefix,
        std::string *error = nullptr);

    const std::string &path() const;

private:
    explicit TemporaryDirectory(std::string path);

    std::string path_;
};

}  // namespace zclaw
