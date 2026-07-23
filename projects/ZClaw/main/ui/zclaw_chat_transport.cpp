#include "zclaw_chat_transport.h"

#include "zclaw_webhook_transport.h"

#include <utility>

namespace zclaw {

ChatTransport::ChatTransport(
    std::shared_ptr<HttpCancellationRegistry> cancellation,
    std::shared_ptr<LhvHttpClient> http_client)
    : cancellation_(std::move(cancellation)),
      http_client_(std::move(http_client))
{
}

bool ChatTransport::send(
    const UiConfig &config, const std::string &message,
    ApprovalHandler approval_handler,
    std::function<void(OperationResult)> completion) const
{
    if (config.bearer_token.empty())
        return WebhookTransport(http_client_).send(
            config, message, std::move(completion));
    return WebSocketTransport(cancellation_).send(
        config, message, std::move(approval_handler), std::move(completion));
}

}  // namespace zclaw
