#include "zclaw_local_binary_installer_backend.h"

#include "zclaw_archive_installer.h"
#include "zclaw_binary_archive_cache.h"
#include "zclaw_file_downloader.h"
#include "zclaw_paths.h"
#include "zclaw_process_executor.h"
#include "zclaw_temporary_directory.h"

#include <filesystem>
#include <memory>
#include <utility>
#include <unistd.h>

namespace zclaw {
namespace {

constexpr const char *kZeroClawReleaseUrl =
    "https://github.com/zeroclaw-labs/zeroclaw/releases/download/v0.8.2/"
    "zeroclaw-aarch64-unknown-linux-gnu.tar.gz";
constexpr const char *kZeroClawArchiveSha256 =
    "d395ccb57e6d94c26a96d565183b7965f326d741244503b4dac2fa7c3124ec14";

}  // namespace

LocalBinaryInstallerBackend::LocalBinaryInstallerBackend(
    std::shared_ptr<HttpCancellationRegistry> cancellation,
    std::shared_ptr<ProcessExecutor> processes,
    std::shared_ptr<LhvHttpClient> http_client)
    : cancellation_(std::move(cancellation)), processes_(std::move(processes)),
      http_client_(std::move(http_client))
{
}

bool LocalBinaryInstallerBackend::prepare_destination(std::string *error) const
{
    const std::filesystem::path directory =
        std::filesystem::path(paths::zeroclaw_binary()).parent_path();
    std::error_code filesystem_error;
    std::filesystem::create_directories(directory, filesystem_error);
    if (!filesystem_error)
        std::filesystem::permissions(
            directory, std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace, filesystem_error);
    if (!filesystem_error)
        return cleanup_binary_install_artifacts(paths::zeroclaw_dir(), error);
    if (error)
        *error = "Cannot prepare ZeroClaw binary directory: " +
                 filesystem_error.message();
    return false;
}

bool LocalBinaryInstallerBackend::binary_ready() const
{
    return std::filesystem::is_regular_file(paths::zeroclaw_binary()) &&
           ::access(paths::zeroclaw_binary().c_str(), X_OK) == 0;
}

bool LocalBinaryInstallerBackend::install(
    std::string *error, const ProgressHandler &progress_handler) const
{
    const std::string cache_directory = paths::zeroclaw_dir();
    const std::string archive_path = paths::zeroclaw_archive();
    bool cached_archive =
        sha256_file_matches(archive_path, kZeroClawArchiveSha256);
    if (!cached_archive) {
        std::error_code remove_error;
        std::filesystem::remove(archive_path, remove_error);
        if (remove_error) {
            if (error)
                *error = "Cannot remove invalid download cache: " +
                         remove_error.message();
            return false;
        }
    }

    std::unique_ptr<TemporaryDirectory> temporary = TemporaryDirectory::create(
        cache_directory, "zeroclaw-install", error);
    if (!temporary)
        return false;

    if (!cached_archive) {
        const std::string partial_path = archive_path + ".part";
        if (progress_handler) {
            progress_handler({BinaryInstallPhase::Downloading, 0, 0, 0.0,
                              kZeroClawReleaseUrl, archive_path});
        }
        if (!FileDownloader(cancellation_).download(
                kZeroClawReleaseUrl, partial_path, error,
                [&progress_handler, &archive_path](
                    const DownloadProgress &progress) {
                    if (progress_handler) {
                        progress_handler({
                            BinaryInstallPhase::Downloading,
                            progress.downloaded_bytes,
                            progress.total_bytes,
                            progress.bytes_per_second,
                            kZeroClawReleaseUrl,
                            archive_path,
                        });
                    }
                })) {
            return false;
        }
        if (!sha256_file_matches(partial_path, kZeroClawArchiveSha256)) {
            std::error_code ignored;
            std::filesystem::remove(partial_path, ignored);
            if (error)
                *error = "Downloaded archive checksum mismatch.";
            return false;
        }
        std::error_code rename_error;
        std::filesystem::rename(partial_path, archive_path, rename_error);
        if (rename_error) {
            std::error_code ignored;
            std::filesystem::remove(partial_path, ignored);
            if (error)
                *error = "Cannot save download cache: " +
                         rename_error.message();
            return false;
        }
    }

    if (progress_handler)
        progress_handler({BinaryInstallPhase::Installing, 0, 0, 0.0});
    ArchiveInstaller installer;
    if (processes_) {
        installer = ArchiveInstaller([processes = processes_](
                                         const ArchiveInstaller::Command &command) {
            return processes->run(command);
        });
    }
    return installer.install_executable(
        archive_path, temporary->path(), "zeroclaw", paths::zeroclaw_binary(), error);
}

}  // namespace zclaw
