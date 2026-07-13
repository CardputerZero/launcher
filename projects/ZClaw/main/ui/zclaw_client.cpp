#include "zclaw_client.h"

#include <httplib.h>

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr const char *ZEROCLAW_RELEASE_URL =
    "https://github.com/zeroclaw-labs/zeroclaw/releases/download/v0.8.2/"
    "zeroclaw-aarch64-unknown-linux-gnu.tar.gz";

struct HttpUrl {
    std::string base;
    std::string path;
};

std::string trim(const std::string &value)
{
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string shell_quote(const std::string &value)
{
    std::string out = "'";
    for (char ch : value) {
        if (ch == '\'')
            out += "'\\''";
        else
            out += ch;
    }
    out += "'";
    return out;
}

std::string json_escape(const std::string &value)
{
    std::string out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (ch < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                out += buf;
            } else {
                out += (char)ch;
            }
        }
    }
    return out;
}

std::string json_unescape(const std::string &value)
{
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            out += value[i];
            continue;
        }
        const char next = value[++i];
        if (next == 'n')
            out += '\n';
        else if (next == 'r')
            out += '\r';
        else if (next == 't')
            out += '\t';
        else
            out += next;
    }
    return out;
}

std::string json_string_field(const std::string &json, const std::string &field)
{
    const std::string needle = "\"" + field + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos)
        return "";
    std::string raw;
    bool escaped = false;
    for (++pos; pos < json.size(); ++pos) {
        char ch = json[pos];
        if (escaped) {
            raw += '\\';
            raw += ch;
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            break;
        } else {
            raw += ch;
        }
    }
    return json_unescape(raw);
}

int json_int_field(const std::string &json, const std::string &field, int fallback)
{
    const std::string needle = "\"" + field + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return fallback;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return fallback;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    int value = 0;
    bool any = false;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        any = true;
        value = value * 10 + (json[pos] - '0');
        ++pos;
    }
    return any ? value : fallback;
}

std::string url_encode(const std::string &value)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char ch : value) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out += (char)ch;
        } else {
            out += '%';
            out += hex[ch >> 4];
            out += hex[ch & 0x0F];
        }
    }
    return out;
}

bool split_http_url(const std::string &url, HttpUrl *out)
{
    const size_t scheme = url.find("://");
    if (scheme == std::string::npos)
        return false;
    const size_t authority = scheme + 3;
    const size_t path = url.find('/', authority);
    if (path == std::string::npos) {
        out->base = url;
        out->path = "/";
    } else {
        out->base = url.substr(0, path);
        out->path = url.substr(path);
        if (out->path.empty())
            out->path = "/";
    }
    return out->base.rfind("http://", 0) == 0 || out->base.rfind("https://", 0) == 0;
}

void configure_http_client(httplib::Client &client, int read_timeout_secs)
{
    client.set_connection_timeout(20);
    client.set_read_timeout(read_timeout_secs);
    client.set_write_timeout(30);
    client.set_follow_location(true);
}

bool file_exists(const std::string &path)
{
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool dir_exists(const std::string &path)
{
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void ensure_zeroclaw_dir()
{
    ::mkdir(ZClawClient::zeroclaw_dir().c_str(), 0700);
}

void ensure_zeroclaw_bin_dir()
{
    ensure_zeroclaw_dir();
    ::mkdir((ZClawClient::zeroclaw_dir() + "/bin").c_str(), 0700);
}

std::string run_command_capture(const std::string &cmd, int *status = nullptr)
{
    std::string output;
    FILE *pipe = ::popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) {
        if (status)
            *status = -1;
        return "failed to start command";
    }
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe))
        output += buffer;
    int rc = ::pclose(pipe);
    if (status)
        *status = rc;
    return trim(output);
}

std::string gateway_http_base(const UiConfig &config)
{
    std::string url = config.webhook_url.empty() ? "http://127.0.0.1:42617/webhook" : config.webhook_url;
    const size_t scheme = url.find("://");
    const size_t path = scheme == std::string::npos ? url.find('/') : url.find('/', scheme + 3);
    if (path != std::string::npos)
        url = url.substr(0, path);
    return url;
}

std::string gateway_ws_url(const UiConfig &config)
{
    std::string base = gateway_http_base(config);
    if (base.rfind("https://", 0) == 0)
        base.replace(0, 5, "ws");
    else if (base.rfind("http://", 0) == 0)
        base.replace(0, 4, "ws");
    return base + "/ws/chat?agent=" + url_encode(config.agent_alias) +
           "&token=" + url_encode(config.bearer_token) +
           "&session_id=zclaw-ui&name=ZClaw";
}

std::string webhook_endpoint(const UiConfig &config)
{
    std::string url = config.webhook_url.empty() ? "http://127.0.0.1:42617/webhook" : config.webhook_url;
    if (url.find('?') == std::string::npos)
        url += "?agent=" + config.agent_alias;
    return url;
}

std::string zeroclaw_cli()
{
    return shell_quote(ZClawClient::zeroclaw_bin_path());
}

void report_setup_progress(const ZClawClient::SetupProgressHandler &handler, const char *status,
                           int percent, size_t downloaded = 0, size_t total = 0, double speed = 0.0)
{
    if (!handler)
        return;
    ZClawSetupProgress progress;
    progress.status = status ? status : "";
    progress.percent = percent;
    progress.downloaded_bytes = downloaded;
    progress.total_bytes = total;
    progress.bytes_per_second = speed;
    handler(progress);
}

bool download_file(const std::string &url_text, const std::string &output_path, std::string *error,
                   const ZClawClient::SetupProgressHandler &progress_handler)
{
    HttpUrl url;
    if (!split_http_url(url_text, &url)) {
        if (error)
            *error = "Invalid download URL.";
        return false;
    }

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error)
            *error = "Cannot write " + output_path;
        return false;
    }

    httplib::Client client(url.base);
    configure_http_client(client, 300);
    auto last_time = std::chrono::steady_clock::now();
    size_t last_bytes = 0;
    auto res = client.Get(url.path, [&](const char *data, size_t len) {
        out.write(data, (std::streamsize)len);
        return (bool)out;
    }, [&](size_t current, size_t total) {
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - last_time).count();
        if (seconds >= 0.2 || current == total) {
            const double speed = seconds > 0.0 ? (double)(current - last_bytes) / seconds : 0.0;
            const int download_percent = total > 0 ? (int)((current * 60) / total) : 0;
            report_setup_progress(progress_handler, "Downloading ZeroClaw", 5 + download_percent,
                                  current, total, speed);
            last_time = now;
            last_bytes = current;
        }
        return true;
    });
    out.close();
    if (!res) {
        if (error)
            *error = "ZeroClaw download failed.\n" + httplib::to_string(res.error());
        return false;
    }
    if (res->status < 200 || res->status >= 300) {
        if (error)
            *error = "ZeroClaw download failed.\nHTTP " + std::to_string(res->status);
        return false;
    }
    return true;
}

bool ensure_zeroclaw_binary(std::string *error,
                            const ZClawClient::SetupProgressHandler &progress_handler)
{
    report_setup_progress(progress_handler, "Checking ZeroClaw", 3);
    ensure_zeroclaw_bin_dir();
    if (file_exists(ZClawClient::zeroclaw_bin_path()) &&
        ::access(ZClawClient::zeroclaw_bin_path().c_str(), X_OK) == 0) {
        report_setup_progress(progress_handler, "ZeroClaw is ready", 65);
        return true;
    }

    const std::string tmp_prefix = ZClawClient::zeroclaw_dir() + "/zeroclaw-install.XXXXXX";
    int rc = 0;
    const std::string tmp_dir = run_command_capture("mktemp -d " + shell_quote(tmp_prefix), &rc);
    if (rc != 0 || tmp_dir.empty()) {
        if (error)
            *error = "Cannot create ZeroClaw install temp dir.";
        return false;
    }

    const std::string archive_path = tmp_dir + "/zeroclaw.tar.gz";
    if (!download_file(ZEROCLAW_RELEASE_URL, archive_path, error, progress_handler)) {
        run_command_capture("rm -rf " + shell_quote(tmp_dir));
        return false;
    }

    report_setup_progress(progress_handler, "Installing ZeroClaw", 67);
    const std::string cmd =
        "tar -xzf " + shell_quote(archive_path) + " -C " + shell_quote(tmp_dir) + " && "
        "found=$(find " + shell_quote(tmp_dir) + " -type f -name zeroclaw | head -n 1) && "
        "test -n \"$found\" && "
        "cp \"$found\" " + shell_quote(ZClawClient::zeroclaw_bin_path()) + " && "
        "chmod 700 " + shell_quote(ZClawClient::zeroclaw_bin_path()) + " && "
        "rm -rf " + shell_quote(tmp_dir);

    const std::string out = run_command_capture(cmd, &rc);
    if (rc != 0 || !file_exists(ZClawClient::zeroclaw_bin_path())) {
        run_command_capture("rm -rf " + shell_quote(tmp_dir));
        if (error)
            *error = "ZeroClaw download failed.\n" + out;
        return false;
    }
    return true;
}

bool config_set(const std::string &path, const std::string &value, std::string *error)
{
    const std::string cmd = zeroclaw_cli() + " config set --no-interactive " +
                            shell_quote(path) + " " + shell_quote(value);
    int rc = 0;
    const std::string out = run_command_capture(cmd, &rc);
    if (rc != 0) {
        if (error)
            *error = "config set failed: " + path + "\n" + out;
        return false;
    }
    return true;
}

bool ensure_default_risk_profile(std::string *error)
{
    const std::string path = ZClawClient::zeroclaw_config_path();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error)
            *error = "Cannot read ZeroClaw config.";
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string config = buffer.str();
    std::istringstream lines(config);
    std::string line;
    while (std::getline(lines, line)) {
        if (trim(line) == "[risk_profiles.default]")
            return true;
    }

    const std::string tmp_path = path + ".zclaw.tmp";
    std::ofstream output(tmp_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        if (error)
            *error = "Cannot update ZeroClaw risk profile.";
        return false;
    }
    output << config;
    if (!config.empty() && config.back() != '\n')
        output << '\n';
    output << "\n[risk_profiles.default]\n";
    output.close();
    if (!output) {
        ::unlink(tmp_path.c_str());
        if (error)
            *error = "Cannot write ZeroClaw risk profile.";
        return false;
    }

    struct stat st {};
    const mode_t mode = ::stat(path.c_str(), &st) == 0 ? st.st_mode & 0777 : 0600;
    ::chmod(tmp_path.c_str(), mode);
    if (::rename(tmp_path.c_str(), path.c_str()) != 0) {
        ::unlink(tmp_path.c_str());
        if (error)
            *error = "Cannot install ZeroClaw risk profile.";
        return false;
    }
    return true;
}

bool ensure_agent(const std::string &alias, std::string *error)
{
    int rc = 0;
    std::string out = run_command_capture(zeroclaw_cli() + " agents list", &rc);
    if (rc != 0) {
        if (error)
            *error = "agents list failed\n" + out;
        return false;
    }
    std::istringstream lines(out);
    std::string line;
    while (std::getline(lines, line)) {
        if (trim(line) == alias)
            return true;
    }
    out = run_command_capture(zeroclaw_cli() + " agents create " + shell_quote(alias), &rc);
    if (rc != 0) {
        if (error)
            *error = "agent creation failed\n" + out;
        return false;
    }
    return true;
}

bool apply_quickstart_config(UiConfig *config, ProviderConfig provider, std::string *error)
{
    if (provider.alias.empty())
        provider.alias = "zclaw";
    if (provider.family.empty())
        provider.family = "openai";
    if (provider.model.empty())
        provider.model = "gpt-4.1-mini";
    if (provider.uri.empty() && provider.family == "ollama")
        provider.uri = "http://127.0.0.1:11434";

    const std::string provider_prefix = "providers.models." + provider.family + "." + provider.alias;
    const std::string provider_ref = provider.family + "." + provider.alias;
    const std::string agent = config->agent_alias.empty() ? "zclaw" : config->agent_alias;

    if (!config_set("gateway.host", "127.0.0.1", error) ||
        !config_set("gateway.port", "42617", error) ||
        !config_set("gateway.require_pairing", "true", error) ||
        !config_set("gateway.request_timeout_secs", "180", error) ||
        !config_set("gateway.long_running_request_timeout_secs", "600", error) ||
        !config_set(provider_prefix + ".model", provider.model, error))
        return false;
    if (!provider.uri.empty() && !config_set(provider_prefix + ".uri", provider.uri, error))
        return false;
    if (!provider.api_key.empty() && !config_set(provider_prefix + ".api_key", provider.api_key, error))
        return false;

    if (!ensure_agent(agent, error) ||
        !ensure_default_risk_profile(error) ||
        !config_set("agents." + agent + ".enabled", "true", error) ||
        !config_set("agents." + agent + ".model_provider", provider_ref, error) ||
        !config_set("agents." + agent + ".risk_profile", "default", error) ||
        !config_set("onboard_state.quickstart_completed", "true", error))
        return false;

    config->agent_alias = agent;
    config->webhook_url = "http://127.0.0.1:42617/webhook";
    return true;
}

std::string extract_pairing_code(const std::string &text)
{
    std::string digits;
    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            digits += ch;
            if (digits.size() >= 6)
                return digits;
        } else {
            digits.clear();
        }
    }
    return "";
}

bool start_zeroclaw_service(std::string *error)
{
    int rc = 0;
    std::string out = run_command_capture(zeroclaw_cli() + " service install", &rc);
    if (rc != 0 && out.find("already") == std::string::npos && out.find("exists") == std::string::npos) {
        if (error)
            *error = "service install failed:\n" + out;
        return false;
    }

    out = run_command_capture(zeroclaw_cli() + " service start", &rc);
    if (rc != 0 && out.find("already") == std::string::npos && out.find("active") == std::string::npos) {
        if (error)
            *error = "service start failed:\n" + out;
        return false;
    }
    return true;
}

std::string generate_pairing_code()
{
    for (int attempt = 0; attempt < 12; ++attempt) {
        int rc = 0;
        const std::string out = run_command_capture(zeroclaw_cli() + " gateway get-paircode --new", &rc);
        if (rc == 0) {
            const std::string code = extract_pairing_code(out);
            if (!code.empty())
                return code;
        }
        ::sleep(1);
    }
    return "";
}

} // namespace

std::string ZClawClient::home_dir()
{
    const char *home = std::getenv("HOME");
    if (home && home[0])
        return home;
    return "/home/nihao";
}

std::string ZClawClient::zeroclaw_dir()
{
    return home_dir() + "/.zeroclaw";
}

std::string ZClawClient::providers_config_path()
{
    return zeroclaw_dir() + "/zclaw_providers.tsv";
}

std::string ZClawClient::ui_config_path()
{
    return zeroclaw_dir() + "/zclaw_ui.tsv";
}

std::string ZClawClient::zeroclaw_config_path()
{
    return zeroclaw_dir() + "/config.toml";
}

std::string ZClawClient::zeroclaw_bin_path()
{
    return zeroclaw_dir() + "/bin/zeroclaw";
}

bool ZClawClient::first_run_needed(const UiConfig &config)
{
    return !dir_exists(zeroclaw_dir()) || !file_exists(zeroclaw_config_path()) ||
           !config.setup_complete || config.bearer_token.empty();
}

void ZClawClient::ensure_storage_dir()
{
    ensure_zeroclaw_dir();
}

bool ZClawClient::has_internet_connection(std::string *error)
{
    static constexpr const char *PROBES[] = {
        "https://www.cloudflare.com/cdn-cgi/trace",
        "https://www.msftconnecttest.com/connecttest.txt",
        "https://github.com/",
    };

    std::string last_error;
    for (const char *probe : PROBES) {
        HttpUrl url;
        if (!split_http_url(probe, &url))
            continue;
        httplib::Client client(url.base);
        client.set_connection_timeout(2);
        client.set_read_timeout(2);
        client.set_write_timeout(2);
        client.set_follow_location(true);
        auto res = client.Get(url.path);
        if (res)
            return true;
        last_error = httplib::to_string(res.error());
    }
    if (error)
        *error = last_error.empty() ? "No public endpoint responded." : last_error;
    return false;
}

ZClawClientResult ZClawClient::pair_with_code(UiConfig config, const std::string &code)
{
    ZClawClientResult result;
    result.config = config;
    HttpUrl url;
    if (!split_http_url(gateway_http_base(config) + "/pair", &url)) {
        result.text = "Pairing request failed.\nInvalid URL.";
        return result;
    }
    httplib::Headers headers = {{"X-Pairing-Code", code}};
    httplib::Client client(url.base);
    configure_http_client(client, 30);
    auto res = client.Post(url.path, headers, "", "application/octet-stream");
    if (!res) {
        result.text = "Pairing request failed.\n" + httplib::to_string(res.error());
        return result;
    }
    const std::string body = res->body;
    if (res->status < 200 || res->status >= 300) {
        result.text = "Pairing request failed.\nHTTP " + std::to_string(res->status) + "\n" + body;
        return result;
    }
    const std::string token = json_string_field(body, "token");
    if (!token.empty()) {
        result.config.bearer_token = token;
        result.text = "Pairing complete.\nWS approvals enabled.";
        result.ok = true;
        return result;
    }
    const std::string error = json_string_field(body, "error");
    result.text = error.empty() ? "Pairing failed.\n" + body : "Pairing failed: " + error;
    return result;
}

ZClawClientResult ZClawClient::send_chat(const UiConfig &config, const std::string &message,
                                         const ApprovalHandler &approval_handler)
{
    ZClawClientResult result;
    result.config = config;

    if (config.bearer_token.empty()) {
        const std::string idempotency = "zclaw-ui-" + std::to_string((unsigned long long)std::time(nullptr));
        const std::string payload = "{\"message\":\"" + json_escape(message) + "\"}";
        HttpUrl url;
        if (!split_http_url(webhook_endpoint(config), &url)) {
            result.text = "Webhook request failed.\nInvalid URL.";
            return result;
        }
        httplib::Headers headers = {{"X-Session-Id", "zclaw-ui"}, {"X-Idempotency-Key", idempotency}};
        if (!config.webhook_secret.empty())
            headers.emplace("X-Webhook-Secret", config.webhook_secret);
        httplib::Client client(url.base);
        configure_http_client(client, 180);
        auto res = client.Post(url.path, headers, payload, "application/json");
        if (!res) {
            result.text = "Webhook request failed.\n" + httplib::to_string(res.error());
            return result;
        }
        const std::string body = res->body;
        if (res->status < 200 || res->status >= 300) {
            result.text = "Webhook request failed.\nHTTP " + std::to_string(res->status) + "\n" + body;
            return result;
        }
        const std::string response = json_string_field(body, "response");
        const std::string error = json_string_field(body, "error");
        result.text = !response.empty() ? response : (!error.empty() ? "ZeroClaw error: " + error : body);
        result.ok = true;
        return result;
    }

    httplib::Headers headers = {{"Sec-WebSocket-Protocol", "zeroclaw.v1"}};
    httplib::ws::WebSocketClient ws(gateway_ws_url(config), headers);
    ws.set_connection_timeout(15);
    ws.set_read_timeout(900);
    ws.set_write_timeout(30);
    if (!ws.is_valid()) {
        result.text = "WS chat failed.\nInvalid WebSocket URL.";
        return result;
    }
    if (!ws.connect()) {
        result.text = "WS chat failed.\nCould not connect to ZeroClaw.";
        return result;
    }
    if (!ws.send("{\"type\":\"message\",\"content\":\"" + json_escape(message) + "\"}")) {
        result.text = "WS chat failed.\nCould not send message.";
        return result;
    }

    std::string chunks;
    std::string final_response;
    const time_t deadline = std::time(nullptr) + 900;
    while (std::time(nullptr) < deadline) {
        std::string event_json;
        const httplib::ws::ReadResult read = ws.read(event_json);
        if (read == httplib::ws::Fail)
            break;
        if (read != httplib::ws::Text || event_json.empty())
            continue;

        const std::string type = json_string_field(event_json, "type");
        if (type == "chunk") {
            chunks += json_string_field(event_json, "content");
        } else if (type == "approval_request") {
            ZClawApprovalRequest request;
            request.request_id = json_string_field(event_json, "request_id");
            request.tool = json_string_field(event_json, "tool");
            request.summary = json_string_field(event_json, "arguments_summary");
            request.timeout_secs = json_int_field(event_json, "timeout_secs", 120);
            const std::string decision = approval_handler ? approval_handler(request) : "deny";
            const std::string response =
                "{\"type\":\"approval_response\",\"request_id\":\"" + json_escape(request.request_id) +
                "\",\"decision\":\"" + json_escape(decision.empty() ? "deny" : decision) + "\"}";
            if (!ws.send(response)) {
                result.text = "WS chat failed.\nCould not send approval.";
                ws.close();
                return result;
            }
        } else if (type == "done") {
            final_response = json_string_field(event_json, "full_response");
            if (final_response.empty())
                final_response = chunks;
            break;
        } else if (type == "error") {
            final_response = "ZeroClaw error: " + json_string_field(event_json, "message");
            break;
        }
    }

    ws.close();
    result.text = !final_response.empty() ? final_response :
                  (chunks.empty() ? "ZeroClaw returned an empty WS response." : chunks);
    result.ok = true;
    return result;
}

ZClawClientResult ZClawClient::run_setup(UiConfig config, const ProviderConfig &provider,
                                         const SetupProgressHandler &progress_handler)
{
    ZClawClientResult result;
    result.config = config;
    std::string error;
    if (!ensure_zeroclaw_binary(&error, progress_handler)) {
        result.text = error;
        return result;
    }
    report_setup_progress(progress_handler, "Configuring provider", 72);
    if (!apply_quickstart_config(&result.config, provider, &error)) {
        result.text = error;
        return result;
    }
    report_setup_progress(progress_handler, "Starting ZeroClaw service", 86);
    if (!start_zeroclaw_service(&error)) {
        result.text = error;
        return result;
    }

    report_setup_progress(progress_handler, "Pairing local client", 94);
    const std::string code = generate_pairing_code();
    if (code.empty()) {
        result.text = "ZeroClaw started, but pairing code generation failed.";
        return result;
    }

    ZClawClientResult pair_result = pair_with_code(result.config, code);
    result.config = pair_result.config;
    if (result.config.bearer_token.empty()) {
        result.text = "Automatic pairing failed.\n" + pair_result.text;
        return result;
    }
    result.config.setup_complete = true;
    result.ok = true;
    report_setup_progress(progress_handler, "Quickstart complete", 100);
    result.text = "Quickstart complete.\nChat and approvals use WS.";
    return result;
}
