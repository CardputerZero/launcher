#pragma once

#include <functional>
#include <string>
#include <vector>

struct ProviderConfig {
    std::string alias;
    std::string family;
    std::string model;
    std::string uri;
    std::string api_key;
};

struct UiConfig {
    std::string webhook_url = "http://127.0.0.1:42617/webhook";
    std::string agent_alias = "zclaw";
    std::string webhook_secret;
    std::string bearer_token;
    bool setup_complete = false;
};

struct ZClawApprovalRequest {
    std::string request_id;
    std::string tool;
    std::string summary;
    int timeout_secs = 120;
};

struct ZClawClientResult {
    std::string text;
    bool ok = false;
    UiConfig config;
};

class ZClawClient {
public:
    using ApprovalHandler = std::function<std::string(const ZClawApprovalRequest &)>;

    static std::string home_dir();
    static std::string zeroclaw_dir();
    static std::string providers_config_path();
    static std::string ui_config_path();
    static std::string zeroclaw_config_path();
    static std::string zeroclaw_bin_path();
    static bool first_run_needed(const UiConfig &config);
    static void ensure_storage_dir();
    static bool has_internet_connection(std::string *error = nullptr);

    ZClawClientResult run_setup(UiConfig config, const ProviderConfig &provider);
    ZClawClientResult pair_with_code(UiConfig config, const std::string &code);
    ZClawClientResult send_chat(const UiConfig &config, const std::string &message,
                                const ApprovalHandler &approval_handler);
};
