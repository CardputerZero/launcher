#pragma once

#include "zclaw_binary_installer_backend.h"

#include <memory>

namespace zclaw {

class HttpCancellationRegistry;
class ProcessExecutor;
class LhvHttpClient;

class LocalBinaryInstallerBackend final : public BinaryInstallerBackend {
public:
    LocalBinaryInstallerBackend() = default;
    explicit LocalBinaryInstallerBackend(
        std::shared_ptr<HttpCancellationRegistry> cancellation,
        std::shared_ptr<ProcessExecutor> processes = nullptr,
        std::shared_ptr<LhvHttpClient> http_client = nullptr);

    bool prepare_destination(std::string *error) const override;
    bool binary_ready() const override;
    bool install(std::string *error,
                 const ProgressHandler &progress_handler) const override;

private:
    std::shared_ptr<HttpCancellationRegistry> cancellation_;
    std::shared_ptr<ProcessExecutor> processes_;
    std::shared_ptr<LhvHttpClient> http_client_;
};

}  // namespace zclaw
