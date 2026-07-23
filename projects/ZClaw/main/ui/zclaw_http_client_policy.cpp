#include "zclaw_http_client_policy.h"

#include <hv/HttpMessage.h>

namespace zclaw {

HttpClientTimeouts http_client_timeouts(HttpClientProfile profile)
{
    switch (profile) {
    case HttpClientProfile::Probe:
        return {2, 2, 2};
    case HttpClientProfile::Pairing:
        return {20, 30, 30};
    case HttpClientProfile::Webhook:
        return {20, 180, 30};
    case HttpClientProfile::Download:
        return {30, 0, 30};
    }
    return {};
}

void configure_http_request(HttpRequest &request, HttpClientProfile profile)
{
    const HttpClientTimeouts timeouts = http_client_timeouts(profile);
    request.connect_timeout = timeouts.connection_seconds;
    request.timeout = timeouts.read_seconds;
    request.redirect = true;
}

}  // namespace zclaw
