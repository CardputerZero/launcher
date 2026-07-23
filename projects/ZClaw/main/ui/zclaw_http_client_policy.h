#pragma once

class HttpRequest;

namespace zclaw {

enum class HttpClientProfile {
    Probe,
    Pairing,
    Webhook,
    Download,
};

struct HttpClientTimeouts {
    int connection_seconds = 0;
    int read_seconds = 0;
    int write_seconds = 0;
};

HttpClientTimeouts http_client_timeouts(HttpClientProfile profile);
void configure_http_request(HttpRequest &request, HttpClientProfile profile);

}  // namespace zclaw
