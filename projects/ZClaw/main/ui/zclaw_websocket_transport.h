#pragma once

#include "zclaw_types.h"

#include <functional>
#include <memory>
#include <string>

namespace zclaw {

class HttpCancellationRegistry;

class WebSocketTransport {
public:
    using ApprovalDecision = std::function<void(std::string)>;
    using ApprovalHandler = std::function<void(
        const ApprovalRequest &, ApprovalDecision)>;

    WebSocketTransport() = default;
    explicit WebSocketTransport(
        std::shared_ptr<HttpCancellationRegistry> cancellation);

    using Completion = std::function<void(OperationResult)>;
    bool send(const UiConfig &config, const std::string &message,
              ApprovalHandler approval_handler,
              Completion completion) const;

private:
    std::shared_ptr<HttpCancellationRegistry> cancellation_;
};

}  // namespace zclaw
