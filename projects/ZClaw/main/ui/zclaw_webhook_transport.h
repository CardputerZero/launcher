#pragma once

#include "zclaw_types.h"

#include <memory>
#include <functional>
#include <string>

namespace zclaw {

class HttpCancellationRegistry;
class LhvHttpClient;

class WebhookTransport {
public:
    explicit WebhookTransport(std::shared_ptr<LhvHttpClient> client);

    using Completion = std::function<void(OperationResult)>;
    bool send(const UiConfig &config, const std::string &message,
              Completion completion) const;

private:
    std::shared_ptr<LhvHttpClient> client_;
};

}  // namespace zclaw
