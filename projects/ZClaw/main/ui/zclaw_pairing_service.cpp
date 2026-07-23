#include "zclaw_pairing_service.h"

#include "zclaw_gateway_response_model.h"
#include "zclaw_http_client_policy.h"
#include "zclaw_lhv_http_client.h"
#include "zclaw_protocol.h"

#include <utility>

namespace zclaw {

PairingService::PairingService(
    std::shared_ptr<LhvHttpClient> client)
    : client_(std::move(client))
{
}

bool PairingService::pair(UiConfig config, const std::string &code,
                          Completion completion) const
{
    OperationResult result;
    result.config = std::move(config);
    if (!completion || !client_)
        return false;
    protocol::HttpUrl url;
    if (!protocol::split_http_url(
            protocol::gateway_http_base(result.config) + "/pair", &url)) {
        result.text = "Pairing request failed.\nInvalid URL.";
        completion(std::move(result));
        return true;
    }

    auto request = std::make_shared<HttpRequest>();
    request->method = HTTP_POST;
    request->url = url.base + url.path;
    request->headers["X-Pairing-Code"] = code;
    request->headers["Content-Type"] = "application/octet-stream";
    configure_http_request(*request, HttpClientProfile::Pairing);
    return client_->send(
        std::move(request),
        [result = std::move(result), completion = std::move(completion)](
            const HttpResponsePtr &response) mutable {
            if (!response) {
                result.text = "Pairing request failed.\nRequest failed.";
                completion(std::move(result));
                return;
            }
            completion(interpret_pairing_response(
                std::move(result.config), response->status_code,
                response->body));
        });
}

}  // namespace zclaw
