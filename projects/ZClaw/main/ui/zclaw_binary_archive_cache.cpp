#include "zclaw_binary_archive_cache.h"

#include <openssl/evp.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>

namespace zclaw {
namespace {

constexpr const char *kInstallPrefix = "zeroclaw-install.";

}  // namespace

bool sha256_file_matches(const std::string &path,
                         const std::string &expected_hex)
{
    std::error_code filesystem_error;
    if (std::filesystem::symlink_status(path, filesystem_error).type() !=
        std::filesystem::file_type::regular)
        return false;

    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    DigestContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
        return false;

    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0 && EVP_DigestUpdate(context.get(), buffer.data(),
                                          static_cast<std::size_t>(count)) != 1)
            return false;
    }
    if (!input.eof())
        return false;

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_size = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &digest_size) != 1)
        return false;
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < digest_size; ++index)
        hex << std::setw(2) << static_cast<unsigned int>(digest[index]);
    return hex.str() == expected_hex;
}

bool cleanup_binary_install_artifacts(const std::string &directory,
                                      std::string *error)
{
    if (error)
        error->clear();
    std::error_code filesystem_error;
    std::filesystem::directory_iterator iterator(directory, filesystem_error);
    if (filesystem_error) {
        if (error)
            *error = "Cannot inspect download cache: " +
                     filesystem_error.message();
        return false;
    }
    for (const std::filesystem::directory_entry &entry : iterator) {
        const std::string name = entry.path().filename().string();
        if (name.rfind(kInstallPrefix, 0) != 0)
            continue;
        std::filesystem::remove_all(entry.path(), filesystem_error);
        if (filesystem_error) {
            if (error)
                *error = "Cannot clear download cache: " +
                         filesystem_error.message();
            return false;
        }
    }
    return true;
}

}  // namespace zclaw
