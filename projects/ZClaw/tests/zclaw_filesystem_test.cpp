#include "zclaw_archive_installer.h"
#include "zclaw_process_runner.h"
#include "zclaw_temporary_directory.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main()
{
    std::string temporary_path;
    {
        std::string error;
        std::unique_ptr<zclaw::TemporaryDirectory> temporary =
            zclaw::TemporaryDirectory::create("/tmp", "zclaw-test", &error);
        assert(temporary);
        assert(error.empty());
        temporary_path = temporary->path();
        assert(std::filesystem::is_directory(temporary_path));
        struct stat temporary_state {};
        assert(::stat(temporary_path.c_str(), &temporary_state) == 0);
        assert((temporary_state.st_mode & 0777) == 0700);
        std::ofstream marker(temporary_path + "/marker");
        assert(marker);
    }
    assert(!std::filesystem::exists(temporary_path));

    std::string error;
    assert(!zclaw::TemporaryDirectory::create(
        "/zclaw-missing-parent", "zclaw-test", &error));
    assert(!error.empty());

    error = "stale";
    assert(!zclaw::TemporaryDirectory::create("/tmp", "../escaped", &error));
    assert(error == "Cannot create temporary directory: Invalid prefix.");
    assert(!zclaw::TemporaryDirectory::create("/tmp", "nested/name", &error));
    assert(!zclaw::TemporaryDirectory::create("/tmp", "", &error));

    std::unique_ptr<zclaw::TemporaryDirectory> source =
        zclaw::TemporaryDirectory::create("/tmp", "zclaw-archive-source");
    std::unique_ptr<zclaw::TemporaryDirectory> destination =
        zclaw::TemporaryDirectory::create("/tmp", "zclaw-archive-destination");
    assert(source && destination);
    const std::string nested = source->path() + "/release/bin";
    assert(std::filesystem::create_directories(nested));
    {
        std::ofstream executable(nested + "/zeroclaw");
        assert(executable);
        executable << "test executable";
    }
    const std::string archive = source->path() + "/release.tar.gz";
    const zclaw::CommandResult packed = zclaw::ProcessRunner::run_shell(
        "tar -czf " + zclaw::ProcessRunner::shell_quote(archive) + " -C " +
        zclaw::ProcessRunner::shell_quote(source->path()) + " release");
    assert(packed.ok());

    const std::string installed = destination->path() + "/zeroclaw";
    zclaw::ArchiveInstaller::Command attempted_command;
    zclaw::ArchiveInstaller rejected(
        [&attempted_command](const zclaw::ArchiveInstaller::Command &command) {
            attempted_command = command;
            return zclaw::CommandResult{1, "cancelled"};
        });
    error.clear();
    assert(!rejected.install_executable(
        archive, destination->path(), "zeroclaw", installed, &error));
    assert((attempted_command == zclaw::ArchiveInstaller::Command{
                                     "tar", "-xzf", archive, "-C",
                                     destination->path()}));
    assert(error == "Cannot extract archive.\ncancelled");

    const std::string outside = source->path() + "/outside-executable";
    {
        std::ofstream executable(outside);
        executable << "must not be installed";
    }
    const std::string symlink_work = destination->path() + "/symlink-work";
    assert(std::filesystem::create_directory(symlink_work));
    assert(::symlink(outside.c_str(),
                     (symlink_work + "/zeroclaw").c_str()) == 0);
    zclaw::ArchiveInstaller no_op(
        [](const zclaw::ArchiveInstaller::Command &) {
            return zclaw::CommandResult{0, ""};
        });
    const std::string symlink_destination = destination->path() + "/from-symlink";
    error = "stale";
    assert(!no_op.install_executable("unused", symlink_work, "zeroclaw",
                                     symlink_destination, &error));
    assert(error == "Archive does not contain zeroclaw.");
    assert(!std::filesystem::exists(symlink_destination));

    const std::string regular_work = destination->path() + "/regular-work";
    assert(std::filesystem::create_directory(regular_work));
    {
        std::ofstream executable(regular_work + "/zeroclaw");
        executable << "replacement executable";
    }
    const std::string protected_target = source->path() + "/protected-target";
    {
        std::ofstream target(protected_target);
        target << "must remain unchanged";
    }
    const std::string linked_destination = destination->path() + "/linked-destination";
    assert(::symlink(protected_target.c_str(), linked_destination.c_str()) == 0);
    error = "stale";
    assert(no_op.install_executable("unused", regular_work, "zeroclaw",
                                    linked_destination, &error));
    assert(error.empty());
    assert(!std::filesystem::is_symlink(linked_destination));
    std::string installed_contents;
    std::ifstream installed_input(linked_destination);
    std::getline(installed_input, installed_contents);
    assert(installed_contents == "replacement executable");
    std::string protected_contents;
    std::ifstream protected_input(protected_target);
    std::getline(protected_input, protected_contents);
    assert(protected_contents == "must remain unchanged");

    error.clear();
    assert(zclaw::ArchiveInstaller().install_executable(
        archive, destination->path(), "zeroclaw", installed, &error));
    assert(error.empty());
    std::ifstream executable(installed);
    std::string contents;
    std::getline(executable, contents);
    assert(contents == "test executable");
    struct stat installed_state {};
    assert(::stat(installed.c_str(), &installed_state) == 0);
    assert((installed_state.st_mode & 0777) == 0700);
    return 0;
}
