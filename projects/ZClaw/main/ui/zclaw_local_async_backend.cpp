#include "zclaw_local_async_backend.h"

#include "zclaw_async_service.h"
#include "zclaw_chat_transport.h"
#include "zclaw_connectivity.h"
#include "zclaw_http_cancellation.h"
#include "zclaw_local_quickstart_backend.h"
#include "zclaw_lhv_http_client.h"
#include "zclaw_pairing_service.h"
#include "zclaw_process_executor.h"
#include "zclaw_quickstart_service.h"

#include <hv/hasync.h>

#include <utility>

namespace zclaw {

LocalAsyncBackend::LocalAsyncBackend()
    : cancellation_(std::make_shared<HttpCancellationRegistry>()),
      processes_(std::make_shared<ProcessExecutor>()),
      http_client_(std::make_shared<LhvHttpClient>()),
      connectivity_(http_client_)
{
}

LocalAsyncBackend::LocalAsyncBackend(std::shared_ptr<ConnectivityProbe> probe)
    : cancellation_(std::make_shared<HttpCancellationRegistry>()),
      processes_(std::make_shared<ProcessExecutor>()),
      http_client_(std::make_shared<LhvHttpClient>()),
      connectivity_(std::move(probe))
{
}

void LocalAsyncBackend::shutdown() noexcept
{
    connectivity_.cancel();
    cancellation_->shutdown();
    processes_->shutdown();
    http_client_->shutdown();
}

bool LocalAsyncBackend::check_network(NetworkCompletion completion) const
{
    if (!completion)
        return false;
    return connectivity_.check(
        [completion = std::move(completion)](bool online,
                                             std::string error) mutable {
            completion({online, std::move(error)});
        });
}

bool LocalAsyncBackend::send_chat(
    UiConfig config, std::string message,
    ApprovalHandler approval_handler, OperationCompletion completion) const
{
    if (!completion)
        return false;
    return ChatTransport(cancellation_, http_client_).send(
        config, message, std::move(approval_handler),
        std::move(completion));
}

bool LocalAsyncBackend::pair(UiConfig config, std::string code,
                             OperationCompletion completion) const
{
    if (!completion)
        return false;
    return PairingService(http_client_).pair(
        std::move(config), code, std::move(completion));
}

OperationResult LocalAsyncBackend::setup(
    UiConfig config, ProviderConfig provider,
    const ProgressHandler &progress_handler) const
{
    const LocalQuickstartBackend backend(cancellation_, processes_, http_client_);
    return QuickstartService(backend).run(
        std::move(config), provider, progress_handler);
}

}  // namespace zclaw
