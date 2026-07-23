#pragma once

#include "zclaw_types.h"
#include "zclaw_websocket_transport.h"

#include <memory>
#include <functional>
#include <string>

namespace zclaw {

class HttpCancellationRegistry;
class LhvHttpClient;

class ChatTransport {
public:
    using ApprovalHandler = WebSocketTransport::ApprovalHandler;

    ChatTransport() = default;
    ChatTransport(std::shared_ptr<HttpCancellationRegistry> cancellation,
                  std::shared_ptr<LhvHttpClient> http_client);

    bool send(const UiConfig &config, const std::string &message,
              ApprovalHandler approval_handler,
              std::function<void(OperationResult)> completion) const;

private:
    std::shared_ptr<HttpCancellationRegistry> cancellation_;
    std::shared_ptr<LhvHttpClient> http_client_;
};

}  // namespace zclaw
