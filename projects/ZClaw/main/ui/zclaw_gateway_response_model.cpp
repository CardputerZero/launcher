#include "zclaw_gateway_response_model.h"

#include "json.hpp"

#include <utility>

namespace zclaw {
namespace {

using Json = nlohmann::json;

const std::string *string_field(const Json &object, const char *field,
                                std::string *storage)
{
    if (!object.is_object())
        return nullptr;
    const auto found = object.find(field);
    if (found == object.end() || !found->is_string())
        return nullptr;
    *storage = found->get<std::string>();
    return storage;
}

}  // namespace

OperationResult interpret_pairing_response(UiConfig config, int http_status,
                                           const std::string &body)
{
    OperationResult result;
    result.config = std::move(config);
    if (http_status < 200 || http_status >= 300) {
        result.text = "Pairing request failed.\nHTTP " +
                      std::to_string(http_status) + "\n" + body;
        return result;
    }

    const Json parsed = Json::parse(body, nullptr, false);
    std::string field;
    const std::string *token = string_field(parsed, "token", &field);
    if (token && !token->empty()) {
        result.config.bearer_token = *token;
        result.text = "Pairing complete.\nWS approvals enabled.";
        result.ok = true;
        return result;
    }

    std::string error_storage;
    const std::string *response_error =
        string_field(parsed, "error", &error_storage);
    result.text = !response_error || response_error->empty()
                      ? "Pairing failed.\n" + body
                      : "Pairing failed: " + *response_error;
    return result;
}

OperationResult interpret_webhook_response(const UiConfig &config, int http_status,
                                           const std::string &body)
{
    OperationResult result;
    result.config = config;
    if (http_status < 200 || http_status >= 300) {
        result.text = "Webhook request failed.\nHTTP " +
                      std::to_string(http_status) + "\n" + body;
        return result;
    }

    const Json parsed = Json::parse(body, nullptr, false);
    std::string field;
    const std::string *response_error = string_field(parsed, "error", &field);
    if (response_error && !response_error->empty()) {
        result.text = "ZeroClaw error: " + *response_error;
        return result;
    }
    std::string response_storage;
    const std::string *reply =
        string_field(parsed, "response", &response_storage);
    result.text = !reply || reply->empty() ? body : *reply;
    result.ok = true;
    return result;
}

}  // namespace zclaw
