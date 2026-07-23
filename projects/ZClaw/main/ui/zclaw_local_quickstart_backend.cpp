#include "zclaw_local_quickstart_backend.h"

#include "zclaw_local_setup_backend.h"
#include "zclaw_pairing_service.h"
#include "zclaw_lhv_http_client.h"
#include "zclaw_quickstart_service.h"

#include <utility>
#include <future>

namespace zclaw {

LocalQuickstartBackend::LocalQuickstartBackend(
    std::shared_ptr<HttpCancellationRegistry> cancellation,
    std::shared_ptr<ProcessExecutor> processes,
    std::shared_ptr<LhvHttpClient> http_client)
    : cancellation_(std::move(cancellation)), processes_(std::move(processes)),
      http_client_(std::move(http_client))
{
}

PreparedSetup LocalQuickstartBackend::prepare(
    UiConfig config, ProviderConfig provider,
    const ProgressHandler &progress_handler) const
{
    const LocalSetupBackend backend(cancellation_, processes_, http_client_);
    return SetupService(backend).prepare(
        std::move(config), std::move(provider), progress_handler);
}

OperationResult LocalQuickstartBackend::pair(
    UiConfig config, const std::string &pairing_code) const
{
    if (!http_client_)
        return {"Pairing request failed.\nHTTP client unavailable.", false,
                std::move(config)};
    auto promise = std::make_shared<std::promise<OperationResult>>();
    std::future<OperationResult> result = promise->get_future();
    if (!PairingService(http_client_).pair(
            std::move(config), pairing_code,
            [promise](OperationResult paired) {
                promise->set_value(std::move(paired));
            })) {
        return {"Pairing request failed.\nRequest was not submitted.", false,
                {}};
    }
    return result.get();
}

}  // namespace zclaw
