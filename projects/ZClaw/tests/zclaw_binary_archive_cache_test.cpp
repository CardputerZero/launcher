#include "zclaw_binary_archive_cache.h"
#include "zclaw_temporary_directory.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void write_file(const std::string &path, const std::string &content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
    assert(output.good());
}

}  // namespace

int main()
{
    std::string error;
    std::unique_ptr<zclaw::TemporaryDirectory> directory =
        zclaw::TemporaryDirectory::create("/tmp", "zclaw-cache-test", &error);
    assert(directory && error.empty());
    const std::string root = directory->path();
    const std::string archive = root + "/zeroclaw.tar.gz";
    write_file(archive, "abc");
    assert(zclaw::sha256_file_matches(
        archive,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    assert(!zclaw::sha256_file_matches(archive, std::string(64, '0')));

    std::filesystem::create_directory(root + "/zeroclaw-install.stale");
    write_file(root + "/zeroclaw-install.stale/data", "partial");
    write_file(root + "/zeroclaw.tar.gz.part", "partial");
    std::filesystem::create_directory(root + "/zeroclaw-installation.keep");
    write_file(root + "/providers.tsv", "keep");
    assert(zclaw::cleanup_binary_install_artifacts(root, &error));
    assert(error.empty());
    assert(!std::filesystem::exists(root + "/zeroclaw-install.stale"));
    assert(std::filesystem::exists(root + "/zeroclaw.tar.gz.part"));
    assert(std::filesystem::exists(root + "/zeroclaw-installation.keep"));
    assert(std::filesystem::exists(root + "/providers.tsv"));
    assert(std::filesystem::exists(archive));

    const std::string external = root + "/external";
    std::filesystem::create_directory(external);
    write_file(external + "/keep", "keep");
    std::filesystem::create_directory_symlink(
        external, root + "/zeroclaw-install.link");
    assert(zclaw::cleanup_binary_install_artifacts(root, &error));
    assert(!std::filesystem::exists(root + "/zeroclaw-install.link"));
    assert(std::filesystem::exists(external + "/keep"));
    return 0;
}
