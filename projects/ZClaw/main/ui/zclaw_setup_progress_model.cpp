#include "zclaw_setup_progress_model.h"

#include <cstdio>

namespace zclaw {
namespace {

std::string format_transfer_detail(const SetupProgress &progress)
{
    if (progress.total_bytes == 0)
        return progress.status;
    char buffer[96];
    const double downloaded_mb = static_cast<double>(progress.downloaded_bytes) /
                                 (1024.0 * 1024.0);
    const double total_mb = static_cast<double>(progress.total_bytes) /
                            (1024.0 * 1024.0);
    std::snprintf(buffer, sizeof(buffer), "%.1f / %.1f MB", downloaded_mb, total_mb);
    return buffer;
}

std::string format_transfer_speed(double bytes_per_second)
{
    if (bytes_per_second <= 0.0)
        return "Waiting for data";
    char buffer[64];
    if (bytes_per_second >= 1024.0 * 1024.0)
        std::snprintf(buffer, sizeof(buffer), "%.1f MB/s",
                      bytes_per_second / (1024.0 * 1024.0));
    else if (bytes_per_second >= 1024.0)
        std::snprintf(buffer, sizeof(buffer), "%.0f KB/s",
                      bytes_per_second / 1024.0);
    else
        std::snprintf(buffer, sizeof(buffer), "%.0f B/s", bytes_per_second);
    return buffer;
}

}  // namespace

SetupProgressPresentation present_setup_progress(const SetupProgress &progress)
{
    SetupProgressPresentation presentation;
    presentation.status = progress.status;
    presentation.percent = progress.percent < 0 ? 0 :
                           (progress.percent > 100 ? 100 : progress.percent);
    presentation.percent_text = std::to_string(presentation.percent) + "%";
    presentation.detail = format_transfer_detail(progress);
    if (!progress.source_url.empty() || progress.total_bytes > 0 ||
        progress.downloaded_bytes > 0 || progress.bytes_per_second > 0.0)
        presentation.speed = format_transfer_speed(progress.bytes_per_second);
    if (!progress.source_url.empty())
        presentation.source_url = progress.source_url;
    if (!progress.destination_path.empty())
        presentation.destination_path = progress.destination_path;
    return presentation;
}

}  // namespace zclaw
