#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

namespace zclaw {

enum class BinaryInstallPhase {
    Downloading,
    Installing,
};

struct BinaryInstallProgress {
    BinaryInstallProgress() = default;
    BinaryInstallProgress(BinaryInstallPhase phase_value,
                          std::size_t downloaded_value = 0,
                          std::size_t total_value = 0,
                          double speed_value = 0.0,
                          std::string source_value = {},
                          std::string destination_value = {})
        : phase(phase_value), downloaded_bytes(downloaded_value),
          total_bytes(total_value), bytes_per_second(speed_value),
          source_url(std::move(source_value)),
          destination_path(std::move(destination_value))
    {
    }

    BinaryInstallPhase phase = BinaryInstallPhase::Downloading;
    std::size_t downloaded_bytes = 0;
    std::size_t total_bytes = 0;
    double bytes_per_second = 0.0;
    std::string source_url;
    std::string destination_path;
};

class BinaryInstallerBackend {
public:
    using ProgressHandler = std::function<void(const BinaryInstallProgress &)>;

    virtual ~BinaryInstallerBackend() = default;

    virtual bool prepare_destination(std::string *error) const = 0;
    virtual bool binary_ready() const = 0;
    virtual bool install(std::string *error,
                         const ProgressHandler &progress_handler) const = 0;
};

}  // namespace zclaw
