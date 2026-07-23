#pragma once

#include "zclaw_types.h"

#include <memory>
#include <functional>
#include <string>

namespace zclaw {

class HttpCancellationRegistry;
class LhvHttpClient;

class PairingService {
public:
    PairingService() = default;
    explicit PairingService(std::shared_ptr<LhvHttpClient> client);

    using Completion = std::function<void(OperationResult)>;
    bool pair(UiConfig config, const std::string &code,
              Completion completion) const;

private:
    std::shared_ptr<LhvHttpClient> client_;
};

}  // namespace zclaw
