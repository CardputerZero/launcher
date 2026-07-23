#include "zclaw_webhook_transport.h"

#include "zclaw_gateway_response_model.h"
#include "zclaw_http_client_policy.h"
#include "zclaw_lhv_http_client.h"
#include "zclaw_protocol.h"
#include "zclaw_request_id.h"

#include <utility>

namespace zclaw {
namespace {

RequestIdGenerator &request_ids()
{
    static RequestIdGenerator generator;
    return generator;
}

}  // namespace

WebhookTransport::WebhookTransport(
    std::shared_ptr<LhvHttpClient> client)
    : client_(std::move(client))
{
}

bool WebhookTransport::send(const UiConfig &config, const std::string &message,
                            Completion completion) const
{
    OperationResult result;
    result.config = config;
    const std::string idempotency = request_ids().next();
    const std::string payload = protocol::make_webhook_message(message);
    protocol::HttpUrl url;
    if (!protocol::split_http_url(protocol::webhook_endpoint(config), &url)) {
        result.text = "Webhook request failed.\nInvalid URL.";
        completion(std::move(result));
        return true;
    }

    auto request = std::make_shared<HttpRequest>();
    request->method = HTTP_POST;
    request->url = url.base + url.path;
    request->body = payload;
    request->headers["Content-Type"] = "application/json";
    request->headers["X-Session-Id"] = "zclaw-ui";
    request->headers["X-Idempotency-Key"] = idempotency;
    if (!config.webhook_secret.empty())
        request->headers["X-Webhook-Secret"] = config.webhook_secret;
    configure_http_request(*request, HttpClientProfile::Webhook);
    if (!client_ || !completion)
        return false;
    return client_->send(
        std::move(request),
        [config, result = std::move(result),
         completion = std::move(completion)](
            const HttpResponsePtr &response) mutable {
            if (!response) {
                result.text = "Webhook request failed.\nRequest failed.";
                completion(std::move(result));
                return;
            }
            completion(interpret_webhook_response(
                config, response->status_code, response->body));
        });
}

}  // namespace zclaw
