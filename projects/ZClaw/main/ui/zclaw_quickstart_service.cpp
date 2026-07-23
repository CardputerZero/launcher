#include "zclaw_quickstart_service.h"

#include "zclaw_quickstart_backend.h"

#include <utility>

namespace zclaw {

QuickstartService::QuickstartService(const QuickstartBackend &backend)
    : backend_(&backend)
{
}

OperationResult QuickstartService::run(
    UiConfig config, const ProviderConfig &provider,
    const ProgressHandler &progress_handler) const
{
    const PreparedSetup prepared = backend_->prepare(
        std::move(config), provider, progress_handler);
    if (!prepared.ok)
        return {prepared.error, false, prepared.config};

    OperationResult result = backend_->pair(prepared.config, prepared.pairing_code);
    if (!result.ok || result.config.bearer_token.empty()) {
        result.text = "Automatic pairing failed.\n" + result.text;
        result.ok = false;
        return result;
    }
    result.config.setup_complete = true;
    result.ok = true;
    if (progress_handler)
        progress_handler({"Quickstart complete", 100});
    result.text = "Quickstart complete.\nChat and approvals use WS.";
    return result;
}

}  // namespace zclaw
