#include "zclaw_binary_installer.h"

#include "zclaw_binary_installer_backend.h"

#include <utility>

namespace zclaw {
namespace {

void report_progress(const BinaryInstaller::ProgressHandler &handler,
                     const char *status, int percent,
                     std::size_t downloaded = 0, std::size_t total = 0,
                     double speed = 0.0, std::string source_url = {},
                     std::string destination_path = {})
{
    if (handler)
        handler({status ? status : "", percent, downloaded, total, speed,
                 std::move(source_url), std::move(destination_path)});
}

void report_backend_progress(const BinaryInstaller::ProgressHandler &handler,
                             const BinaryInstallProgress &progress)
{
    if (progress.phase == BinaryInstallPhase::Installing) {
        report_progress(handler, "Installing ZeroClaw", 67);
        return;
    }
    const int percent = progress.total_bytes > 0
                            ? static_cast<int>((progress.downloaded_bytes * 60) /
                                               progress.total_bytes)
                            : 0;
    report_progress(handler, "Downloading ZeroClaw", 5 + percent,
                    progress.downloaded_bytes, progress.total_bytes,
                    progress.bytes_per_second, progress.source_url,
                    progress.destination_path);
}

}  // namespace

BinaryInstaller::BinaryInstaller(std::shared_ptr<BinaryInstallerBackend> backend)
    : backend_(std::move(backend))
{
}

bool BinaryInstaller::ensure_installed(
    std::string *error, const ProgressHandler &progress_handler) const
{
    report_progress(progress_handler, "Checking ZeroClaw", 3);
    if (!backend_) {
        if (error)
            *error = "Missing binary installer backend.";
        return false;
    }
    if (!backend_->prepare_destination(error))
        return false;
    if (backend_->binary_ready()) {
        report_progress(progress_handler, "ZeroClaw is ready", 65);
        return true;
    }
    return backend_->install(
        error, [&progress_handler](const BinaryInstallProgress &progress) {
            report_backend_progress(progress_handler, progress);
        });
}

}  // namespace zclaw
