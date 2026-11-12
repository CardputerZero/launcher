/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_system_page.hpp"

#include "cp0_lvgl_app.h"
#include "cp0_font_service.hpp"
#include "hal_lvgl_bsp.h"
#include "input_keys.h"
#include "keyboard_input.h"
#include "settings_about_info_model.hpp"
#include "settings_software_model.hpp"
#include "settings_http_worker.hpp"
#include "settings_software_updater.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <functional>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

enum class TimerInterval : uint32_t {
    UpdatePollMs = 500,
};
constexpr std::chrono::seconds kUpdateTimeout{20 * 60};

#define SETTINGS_SYSTEM_STRINGIFY_IMPL(value) #value
#define SETTINGS_SYSTEM_STRINGIFY(value) SETTINGS_SYSTEM_STRINGIFY_IMPL(value)

const char *launcher_version()
{
#ifdef LAUNCHER_VERSION
    return LAUNCHER_VERSION;
#elif defined(LAUNCHER_VERSION_RAW)
    return SETTINGS_SYSTEM_STRINGIFY(LAUNCHER_VERSION_RAW);
#else
    return "0.0.0-dev";
#endif
}

const char *launcher_channel()
{
#ifdef LAUNCHER_CHANNEL
    return LAUNCHER_CHANNEL;
#elif defined(LAUNCHER_CHANNEL_RAW)
    return SETTINGS_SYSTEM_STRINGIFY(LAUNCHER_CHANNEL_RAW);
#else
    return "development";
#endif
}

const char *launcher_build_date()
{
#ifdef LAUNCHER_BUILD_DATE
    return LAUNCHER_BUILD_DATE;
#elif defined(LAUNCHER_BUILD_DATE_RAW)
    return SETTINGS_SYSTEM_STRINGIFY(LAUNCHER_BUILD_DATE_RAW);
#else
    return "unknown";
#endif
}

const char *launcher_commit()
{
#ifdef LAUNCHER_GIT_COMMIT
    return LAUNCHER_GIT_COMMIT;
#elif defined(LAUNCHER_GIT_COMMIT_RAW)
    return SETTINGS_SYSTEM_STRINGIFY(LAUNCHER_GIT_COMMIT_RAW);
#else
    return "unknown";
#endif
}

std::string lvgl_version()
{
    return std::to_string(LVGL_VERSION_MAJOR) + "." +
           std::to_string(LVGL_VERSION_MINOR) + "." +
           std::to_string(LVGL_VERSION_PATCH);
}

const lv_font_t *title_font()
{
    return settings_fonts::sans(14, LV_FREETYPE_FONT_STYLE_BOLD);
}

const lv_font_t *body_font()
{
    return settings_fonts::sans(12);
}

const lv_font_t *hint_font()
{
    return settings_fonts::sans(12, LV_FREETYPE_FONT_STYLE_BOLD);
}

lv_obj_t *create_label(lv_obj_t *parent,
                       int x,
                       int y,
                       int width,
                       const char *text,
                       uint32_t color,
                       const lv_font_t *font,
                       bool wrap = false)
{
    if (!parent) return nullptr;
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, wrap ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font ? font : body_font(), LV_PART_MAIN);
    return label;
}

void configure_page(lv_obj_t *object, int width, int height)
{
    if (!object) return;
    lv_obj_set_size(object, width, height);
    lv_obj_set_pos(object, 0, 0);
    lv_obj_set_style_bg_color(object, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, 0, LV_PART_MAIN);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void set_label(lv_obj_t *label, const std::string &text)
{
    if (label) lv_label_set_text(label, text.c_str());
}

bool uses_network_table(SettingsSystemPageKind kind)
{
    return kind == SettingsSystemPageKind::Network ||
           kind == SettingsSystemPageKind::Ethernet;
}

lv_obj_t *create_network_table_row(lv_obj_t *parent,
                                   int row_index,
                                   const char *key)
{
    if (!parent) return nullptr;
    using LayoutMetric = LvSettingSystemInfoPage3::LayoutMetric;
    const auto metric = [](LayoutMetric value) {
        return LvSettingSystemInfoPage3::metric(value);
    };

    lv_obj_t *row = lv_obj_create(parent);
    if (!row) return nullptr;
    lv_obj_set_pos(row,
                   metric(LayoutMetric::TableX),
                   metric(LayoutMetric::TableY) + row_index * metric(LayoutMetric::TableRowH));
    lv_obj_set_size(row, metric(LayoutMetric::TableW), metric(LayoutMetric::TableRowH));
    lv_obj_set_style_bg_color(row,
                              lv_color_hex(row_index % 2 == 0 ? 0x151515 : 0x1B1B1B),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(0x383838), LV_PART_MAIN);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *divider = lv_obj_create(row);
    if (divider) {
        lv_obj_set_pos(divider, metric(LayoutMetric::TableDividerX), 0);
        lv_obj_set_size(divider,
                        metric(LayoutMetric::TableDividerW),
                        metric(LayoutMetric::TableDividerH));
        lv_obj_set_style_bg_color(divider, lv_color_hex(0x505050), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);
        lv_obj_remove_flag(divider, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    }

    create_label(row,
                 metric(LayoutMetric::TableKeyX),
                 4,
                 metric(LayoutMetric::TableKeyW),
                 key,
                 0xA8A8A8,
                 settings_fonts::sans(13, LV_FREETYPE_FONT_STYLE_BOLD));
    return create_label(row,
                        metric(LayoutMetric::TableValueX),
                        3,
                        metric(LayoutMetric::TableValueW),
                        "--",
                        0xF2F2F2,
                        settings_fonts::mono(14));
}

SettingsSystemPageKind resolve_kind(const NodeIter &page_node, SettingsSystemPageKind requested)
{
    if (requested != SettingsSystemPageKind::Auto) return requested;
    if (page_node.node && !page_node->label.empty()) {
        if (page_node->label == "Password") return SettingsSystemPageKind::Password;
        if (page_node->label == "OS") return SettingsSystemPageKind::OS;
        if (page_node->label == "Username" || page_node->label == "Hostname" ||
            page_node->label == "Account")
            return SettingsSystemPageKind::Account;
        if (page_node->label == "Version") return SettingsSystemPageKind::Version;
        if (page_node->label == "Build") return SettingsSystemPageKind::Build;
        if (page_node->label == "Ethernet" || page_node->label == "IP" ||
            page_node->label == "Gateway" || page_node->label == "MAC")
            return SettingsSystemPageKind::Ethernet;
    }
    return SettingsSystemPageKind::Network;
}

bool is_update_page_label(const NodeIter &page_node)
{
    if (!page_node.node) return false;
    return page_node->label == "Update" || page_node->label == "Update Launcher" ||
           page_node->label == "System update" || page_node->label == "Check system";
}

// Keep the OS-info transport private to this page.  The page is the only
// consumer, so exposing a separate API surface only added an unnecessary
// coupling between settings modules.
void request_osinfo(std::list<std::string> arguments,
                    std::function<void(int, std::string)> callback) noexcept
{
    if (!callback) return;

    auto delivered = std::make_shared<std::atomic_bool>(false);
    auto safe_callback = [callback = std::move(callback), delivered](int code,
                                                                       std::string data) mutable {
        bool expected = false;
        if (!delivered->compare_exchange_strong(expected, true)) return;
        try {
            callback(code, std::move(data));
        } catch (...) {
        }
    };

    try {
        cp0_signal_osinfo_api(std::move(arguments), safe_callback);
    } catch (...) {
        safe_callback(-1, "osinfo service unavailable");
    }
}

std::string launcher_release_url()
{
    const char *configured = std::getenv("APPLAUNCH_UPDATE_RELEASE_URL");
    return configured && configured[0]
        ? configured
        : "https://github.com/CardputerZero/launcher/releases/download/launcher-latest";
}

std::string app_update_status_path()
{
    const char *configured = std::getenv("APPLAUNCH_UPDATE_STATE_DIR");
    const std::string root = configured && configured[0]
        ? configured : "/var/lib/applaunch-updater";
    return root + "/apps-status";
}

std::string read_app_update_status()
{
    std::ifstream input(app_update_status_path());
    std::string status;
    if (!std::getline(input, status)) return {};
    return status;
}

int capture_process(const std::vector<std::string> &arguments, std::string &output)
{
    output.clear();
    std::list<std::string> request{"CaptureArgv", "timeout", "30"};
    for (const std::string &argument : arguments) request.push_back(argument);
    int code = -1;
    try {
        cp0_signal_process_api(
            std::move(request), [&](int result, std::string data) {
                code = result;
                output = std::move(data);
            });
    } catch (...) {
        return -1;
    }
    return code;
}

struct CachedCurl {
    int code = -1;
    std::string output;
};

using CurlCache = std::map<std::string, CachedCurl>;

void cache_http_response(CurlCache &cache, const std::string &url,
                         const settings_http::Response &response,
                         bool output_effective_url)
{
    CachedCurl cached;
    if (response.ok) {
        cached.code = 0;
        cached.output = output_effective_url
            ? settings_software::trim(response.effective_url)
            : response.body;
    } else {
        cached.code = response.code != 0 ? response.code : 22;
    }
    cache[url] = std::move(cached);
}

void prefetch_software_http(const std::vector<settings_software::AppSpec> &specs,
                            const std::atomic<bool> &cancel,
                            const std::function<void(int)> &progress,
                            CurlCache &cache)
{
    if (cancel.load(std::memory_order_acquire)) return;

    std::vector<settings_http::Request> api_requests;
    api_requests.reserve(specs.size() + 1);
    for (const auto &spec : specs) {
        settings_http::Request request;
        request.url = "https://api.github.com/repos/CardputerZero/" + spec.repo +
                      "/releases/latest";
        request.accept_header = "application/vnd.github+json";
        request.timeout_ms = 15000;
        api_requests.push_back(std::move(request));
    }
    settings_http::Request launcher_request;
    launcher_request.url = launcher_release_url() + "/applaunch_arm64.deb.update-info";
    launcher_request.timeout_ms = 20000;
    const std::string launcher_request_url = launcher_request.url;
    api_requests.push_back(std::move(launcher_request));

    if (progress) progress(10);
    const std::vector<settings_http::Response> api_responses =
        settings_http::download_batch(api_requests, &cancel, 6);
    if (cancel.load(std::memory_order_acquire)) return;
    if (progress) progress(35);

    std::vector<settings_software::AppSpec> fallback_specs;
    for (std::size_t i = 0; i < specs.size() && i < api_responses.size(); ++i) {
        const auto &response = api_responses[i];
        cache_http_response(cache, api_requests[i].url, response, false);
        std::string version;
        if (!response.ok ||
            !settings_software::parse_asset_version(response.body, specs[i], version))
            fallback_specs.push_back(specs[i]);
    }
    if (!api_responses.empty())
        cache_http_response(cache, launcher_request_url, api_responses.back(), false);

    std::vector<settings_http::Request> latest_requests;
    for (const auto &spec : fallback_specs) {
        settings_http::Request request;
        request.url = "https://github.com/CardputerZero/" + spec.repo +
                      "/releases/latest";
        request.timeout_ms = 15000;
        request.want_effective_url = true;
        latest_requests.push_back(std::move(request));
    }

    if (cancel.load(std::memory_order_acquire)) return;
    const std::vector<settings_http::Response> latest_responses =
        settings_http::download_batch(latest_requests, &cancel, 6);
    if (cancel.load(std::memory_order_acquire)) return;
    if (progress) progress(60);

    std::vector<settings_http::Request> asset_pages;
    std::vector<std::string> asset_urls;
    for (std::size_t i = 0; i < fallback_specs.size() &&
                            i < latest_responses.size();
         ++i) {
        const auto &response = latest_responses[i];
        cache_http_response(cache, latest_requests[i].url, response, true);
        if (!response.ok) continue;
        const std::string effective = settings_software::trim(response.effective_url);
        const std::size_t tag_position = effective.rfind("/tag/");
        if (tag_position == std::string::npos) continue;
        const std::string tag = effective.substr(tag_position + 5);
        if (tag.empty()) continue;
        settings_http::Request request;
        request.url = "https://github.com/CardputerZero/" + fallback_specs[i].repo +
                      "/releases/expanded_assets/" + tag;
        request.timeout_ms = 15000;
        asset_urls.push_back(request.url);
        asset_pages.push_back(std::move(request));
    }

    const std::vector<settings_http::Response> asset_responses =
        settings_http::download_batch(asset_pages, &cancel, 6);
    if (cancel.load(std::memory_order_acquire)) return;
    for (std::size_t i = 0; i < asset_urls.size() && i < asset_responses.size(); ++i)
        cache_http_response(cache, asset_urls[i], asset_responses[i], false);
    if (progress) progress(70);
}

template <typename Owner>
bool start_process_request(Owner *owner,
                           std::list<std::string> arguments,
                           std::function<void(int, std::string)> handler,
                           std::function<void(int, std::string)> stale_handler = {})
{
    if (!owner || !handler || !owner->ensure_async_dispatch()) return false;
    const auto token = owner->async_token();
    if (!token.valid()) return false;

    auto callback = [token,
                     handler = std::move(handler),
                     stale_handler = std::move(stale_handler)](int code,
                                                                std::string data) mutable {
        auto deliver_stale = [&stale_handler](int result_code, std::string result_data) {
            if (!stale_handler) return;
            try {
                stale_handler(result_code, std::move(result_data));
            } catch (...) {
            }
        };
        if (!token.valid()) {
            deliver_stale(code, std::move(data));
            return;
        }
        const std::string stale_data = data;
        const bool queued = SettingsAsync::Dispatch::enqueue_from_callback(
            token,
            [handler = std::move(handler), code, data = std::move(data)]() mutable {
                if (handler) handler(code, std::move(data));
            });
        if (!queued) deliver_stale(code, stale_data);
    };

    std::list<std::string> request{"CaptureArgv", "timeout", "30"};
    request.splice(request.end(), arguments);
    return owner->async_tasks().start(
        [request = std::move(request), callback = std::move(callback)](auto stop) mutable {
            if (stop && stop->load(std::memory_order_acquire)) return;
            try {
                cp0_signal_process_api(request, callback);
            } catch (...) {
                callback(-1, "process api unavailable");
            }
        });
}

template <typename Owner>
bool start_osinfo_request(Owner *owner,
                          std::list<std::string> arguments,
                          std::function<void(int, std::string)> handler,
                          std::function<void(int, std::string)> stale_handler = {})
{
    if (!owner || !handler || !owner->ensure_async_dispatch()) return false;

    const auto token = owner->async_token();
    if (!token.valid()) return false;

    auto callback = [token,
                    handler = std::move(handler),
                    stale_handler = std::move(stale_handler)](int code,
                                                                std::string data) mutable {
        auto deliver_stale = [&stale_handler](int result_code, std::string result_data) {
            if (!stale_handler) return;
            try {
                stale_handler(result_code, std::move(result_data));
            } catch (...) {
            }
        };

        if (!token.valid()) {
            deliver_stale(code, std::move(data));
            return;
        }

        const std::string stale_data = data;
        const bool queued = SettingsAsync::Dispatch::enqueue_from_callback(
            token,
            [handler = std::move(handler), code, data = std::move(data)]() mutable {
                if (handler) handler(code, std::move(data));
            });
        if (!queued) deliver_stale(code, stale_data);
    };

    return owner->async_tasks().start(
        [arguments = std::move(arguments), callback = std::move(callback)](auto stop) mutable {
            if (stop && stop->load(std::memory_order_acquire)) return;
            request_osinfo(std::move(arguments), std::move(callback));
        });
}

void stop_timer(lv_timer_t *&timer)
{
    if (!timer) return;
    lv_timer_delete(timer);
    timer = nullptr;
}

} // namespace

struct LvSettingSystemInfoPage3::State
{
    SettingsSystemPageKind kind = SettingsSystemPageKind::Network;
    uint64_t generation = 1;
    bool request_pending = false;
    bool used_default_fallback = false;
    std::chrono::steady_clock::time_point request_deadline;
    lv_timer_t *request_timer = nullptr;
    settings_system::NetworkInfo network{"--", "--", "--"};
    settings_system::AccountInfo account{"--", "--"};
    SettingsAboutOsInfo os_info;
    std::string status;
    lv_obj_t *title = nullptr;
    lv_obj_t *line_one = nullptr;
    lv_obj_t *line_two = nullptr;
    lv_obj_t *line_three = nullptr;
    lv_obj_t *status_label = nullptr;
    lv_obj_t *hint = nullptr;
};

namespace {

constexpr std::chrono::seconds kInfoRequestTimeout{15};

void render_system_info(LvSettingSystemInfoPage3 *page);

template <typename StateT>
void stop_system_request_timer(StateT *state)
{
    if (!state) return;
    stop_timer(state->request_timer);
}

void system_info_request_timeout_cb(lv_timer_t *timer);

bool arm_system_request_timer(LvSettingSystemInfoPage3 *page)
{
    if (!page || !page->state_) return false;
    auto *state = page->state_.get();
    stop_system_request_timer(state);
    state->request_deadline = std::chrono::steady_clock::now() + kInfoRequestTimeout;
    state->request_timer = lv_timer_create(system_info_request_timeout_cb, 250, page);
    return state->request_timer != nullptr;
}

void system_info_request_timeout_cb(lv_timer_t *timer)
{
    auto *page = timer ? static_cast<LvSettingSystemInfoPage3 *>(lv_timer_get_user_data(timer)) : nullptr;
    if (!page || !page->state_ || timer != page->state_->request_timer) return;
    auto *state = page->state_.get();
    if (!state->request_pending) {
        stop_system_request_timer(state);
        return;
    }
    if (std::chrono::steady_clock::now() < state->request_deadline) return;

    stop_system_request_timer(state);
    state->request_pending = false;
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    page->advance_async_generation();
    state->status = settings_system::backend_error_label(-ETIMEDOUT);
    render_system_info(page);
}

void render_system_info(LvSettingSystemInfoPage3 *page)
{
    if (!page || !page->Get()) return;
    auto *state = page->state_.get();
    if (!state) return;

    switch (state->kind) {
    case SettingsSystemPageKind::Network:
    case SettingsSystemPageKind::Ethernet:
        set_label(state->title, "Ethernet Info");
        set_label(state->line_one, state->network.ip);
        set_label(state->line_two, state->network.gateway);
        set_label(state->line_three, state->network.mac);
        break;
    case SettingsSystemPageKind::Account:
        set_label(state->title, "Account");
        set_label(state->line_one, "Username: " + state->account.username);
        set_label(state->line_two, "Password: " + settings_system::password_unsupported_label());
        set_label(state->line_three, "Hostname: " + state->account.hostname);
        break;
    case SettingsSystemPageKind::Password:
        set_label(state->title, "Account password");
        set_label(state->line_one, settings_system::password_unsupported_label());
        set_label(state->line_two, "The backend exposes read-only account information.");
        set_label(state->line_three, "No password is changed or stored by Settings.");
        break;
    case SettingsSystemPageKind::OS:
        set_label(state->title, "OS");
        set_label(state->line_one, "Build date: " + state->os_info.build_date);
        set_label(state->line_two, "Commit: " + state->os_info.commit);
        set_label(state->line_three, "");
        break;
    case SettingsSystemPageKind::Version:
        set_label(state->title, "Version");
        set_label(state->line_one, settings_system::version_label(launcher_version()));
        set_label(state->line_two, settings_system::build_label(
                                      launcher_build_date(), launcher_channel(), launcher_commit()));
        set_label(state->line_three, std::string("Commit: ") + launcher_commit());
        break;
    case SettingsSystemPageKind::Build:
        set_label(state->title, "Build");
        set_label(state->line_one, settings_system::build_label(
                                      launcher_build_date(), launcher_channel(), launcher_commit()));
        set_label(state->line_two, std::string("Build date: ") + launcher_build_date());
        set_label(state->line_three, std::string("Commit: ") + launcher_commit());
        break;
    case SettingsSystemPageKind::Auto:
        break;
    }

    // Keep machine-readable values (addresses, versions and commits) in a
    // fixed-width face while account and explanatory text retain CJK coverage.
    const bool network_table = uses_network_table(state->kind);
    const bool machine_values = network_table ||
                                state->kind == SettingsSystemPageKind::OS ||
                                state->kind == SettingsSystemPageKind::Version ||
                                state->kind == SettingsSystemPageKind::Build;
    const lv_font_t *line_font = network_table
        ? settings_fonts::mono(14)
        : machine_values ? settings_fonts::mono(11) : settings_fonts::cjk_sans(12);
    for (lv_obj_t *line : {state->line_one, state->line_two, state->line_three}) {
        if (!line) continue;
        lv_obj_set_style_text_font(line, line_font, LV_PART_MAIN);
        // Hashes and other machine values can be wider than the fixed text box.
        // Scroll them in place so the complete value remains readable.
        lv_label_set_long_mode(line, machine_values ? LV_LABEL_LONG_SCROLL_CIRCULAR
                                                    : LV_LABEL_LONG_CLIP);
    }

    set_label(state->status_label, state->status);
    if (state->hint) {
        const char *hint = state->request_pending ? "Loading..." : "ENTER: refresh   ESC: back";
        lv_label_set_text(state->hint, hint);
    }
}

void set_system_error(LvSettingSystemInfoPage3 *page, int code, const std::string &payload)
{
    if (!page || !page->state_) return;
    if (code == 0)
        page->state_->status = "Invalid system information payload";
    else
        page->state_->status = settings_system::backend_error_label(code, payload);
    render_system_info(page);
}

void request_network(LvSettingSystemInfoPage3 *page)
{
    if (!page || !page->state_ || page->state_->request_pending) return;
    auto *state = page->state_.get();
    page->advance_async_generation();
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    const uint64_t request_generation = state->generation;
    state->request_pending = true;
    state->status = "Loading Ethernet information...";
    state->used_default_fallback = false;
    if (!arm_system_request_timer(page)) {
        state->request_pending = false;
        state->status = "Unable to create Ethernet timeout timer";
        render_system_info(page);
        return;
    }
    render_system_info(page);

    const bool use_default = state->kind == SettingsSystemPageKind::Network;
    const std::string command = use_default ? "NetworkDefaultInfoRead" : "EthInfoRead";
    if (!start_osinfo_request(
            page,
            {command},
            [page, request_generation, use_default](int code, std::string payload) {
                auto *state = page ? page->state_.get() : nullptr;
                if (!state || request_generation != state->generation || !page->Get()) return;
                stop_system_request_timer(state);
                state->request_pending = false;
                settings_system::NetworkInfo candidate;
                if (code == 0 && settings_system::parse_network_info_strict(payload, candidate)) {
                    state->network = std::move(candidate);
                    state->status = "Ethernet information loaded";
                    render_system_info(page);
                    return;
                }

                if (!use_default && !state->used_default_fallback) {
                    state->used_default_fallback = true;
                    state->request_pending = true;
                    state->status = "Trying default network information...";
                    page->advance_async_generation();
                    const uint64_t fallback_generation = ++state->generation;
                    if (!arm_system_request_timer(page)) {
                        state->request_pending = false;
                        set_system_error(page, -1, "Unable to create Ethernet timeout timer");
                        return;
                    }
                    render_system_info(page);
                    if (start_osinfo_request(
                            page,
                            {"NetworkDefaultInfoRead"},
                            [page, fallback_generation](int fallback_code,
                                                         std::string fallback_payload) {
                                auto *fallback_state = page ? page->state_.get() : nullptr;
                                if (!fallback_state || fallback_generation != fallback_state->generation ||
                                    !page->Get())
                                    return;
                                stop_system_request_timer(fallback_state);
                                fallback_state->request_pending = false;
                                settings_system::NetworkInfo fallback_info;
                                if (fallback_code == 0 &&
                                    settings_system::parse_network_info_strict(fallback_payload, fallback_info)) {
                                    fallback_state->network = std::move(fallback_info);
                                    fallback_state->status = "Ethernet information loaded";
                                    render_system_info(page);
                                } else {
                                    set_system_error(page, fallback_code, fallback_payload);
                                }
                            }))
                        return;
                    state->request_pending = false;
                    stop_system_request_timer(state);
                }
                set_system_error(page, code, payload);
            })) {
        state->request_pending = false;
        stop_system_request_timer(state);
        state->status = "Unable to schedule Ethernet request";
        render_system_info(page);
    }
}

void request_account(LvSettingSystemInfoPage3 *page)
{
    if (!page || !page->state_ || page->state_->request_pending) return;
    auto *state = page->state_.get();
    page->advance_async_generation();
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    const uint64_t request_generation = state->generation;
    state->request_pending = true;
    state->status = "Loading account information...";
    if (!arm_system_request_timer(page)) {
        state->request_pending = false;
        state->status = "Unable to create account timeout timer";
        render_system_info(page);
        return;
    }
    render_system_info(page);

    if (!start_osinfo_request(
            page,
            {"AccountInfoRead"},
            [page, request_generation](int code, std::string payload) {
                auto *state = page ? page->state_.get() : nullptr;
                if (!state || request_generation != state->generation || !page->Get()) return;
                stop_system_request_timer(state);
                state->request_pending = false;
                settings_system::AccountInfo candidate;
                if (code == 0 && settings_system::parse_account_info_strict(payload, candidate)) {
                    state->account = std::move(candidate);
                    state->status = "Account information loaded";
                    render_system_info(page);
                } else {
                    set_system_error(page, code, payload);
                }
            })) {
        state->request_pending = false;
        stop_system_request_timer(state);
        state->status = "Unable to schedule account request";
        render_system_info(page);
    }
}

void refresh_system_info(LvSettingSystemInfoPage3 *page)
{
    if (!page || !page->state_) return;
    switch (page->state_->kind) {
    case SettingsSystemPageKind::Network:
    case SettingsSystemPageKind::Ethernet: request_network(page); break;
    case SettingsSystemPageKind::Account: request_account(page); break;
    case SettingsSystemPageKind::Password:
        page->state_->status = "Read-only account information";
        render_system_info(page);
        break;
    case SettingsSystemPageKind::OS:
        page->state_->os_info = SettingsAboutInfoModel::read_os_issue_file();
        if (page->state_->os_info.build_date == "unknown" &&
            page->state_->os_info.commit == "unknown")
            page->state_->status = "OS build information unavailable";
        else if (page->state_->os_info.build_date == "unknown" ||
                 page->state_->os_info.commit == "unknown")
            page->state_->status = "OS build information is incomplete";
        else
            page->state_->status = "OS build information loaded";
        render_system_info(page);
        break;
    case SettingsSystemPageKind::Version:
    case SettingsSystemPageKind::Build:
        page->state_->status = "Build information loaded";
        render_system_info(page);
        break;
    case SettingsSystemPageKind::Auto: break;
    }
}

void system_info_key_event(LvSettingSystemInfoPage3 *page, lv_event_t *event)
{
    if (!page || !event || lv_event_get_code(event) != LV_EVENT_KEY) return;
    const uint32_t key = lv_event_get_key(event);
    if (key == LV_KEY_ESC || key == LV_KEY_LEFT) {
        if (page->state_) {
            ++page->state_->generation;
            page->state_->request_pending = false;
            stop_system_request_timer(page->state_.get());
            page->advance_async_generation();
        }
        if (page->LeaveSelfPage) page->LeaveSelfPage();
    } else if (key == LV_KEY_ENTER || key == LV_KEY_RIGHT) {
        refresh_system_info(page);
    }
    lv_event_stop_processing(event);
}

} // namespace

LvSettingSystemInfoPage3::LvSettingSystemInfoPage3(lv_obj_t *parent,
                                                   const NodeIter &page_node,
                                                   std::function<void()> back_callback,
                                                   SettingsSystemPageKind kind)
    : state_(std::make_unique<State>())
{
    state_->kind = resolve_kind(page_node, kind);
    LeaveSelfPage = std::move(back_callback);
    create_ui(parent);
}

LvSettingSystemInfoPage3::~LvSettingSystemInfoPage3()
{
    if (state_) {
        ++state_->generation;
        state_->request_pending = false;
        stop_system_request_timer(state_.get());
    }
    cancel_async_tasks();
    if (ComponensObj) {
        lv_obj_delete(ComponensObj);
        ComponensObj = nullptr;
    }
    state_.reset();
}

void LvSettingSystemInfoPage3::AnimateNextIn(std::function<void()> animate_over_func)
{
    if (animate_over_func) animate_over_func();
}

void LvSettingSystemInfoPage3::AnimateNextOut(std::function<void()> animate_over_func)
{
    if (animate_over_func) animate_over_func();
}

void LvSettingSystemInfoPage3::LoadNextPage()
{
}

void LvSettingSystemInfoPage3::LeaveNextPage()
{
    if (state_) {
        ++state_->generation;
        if (state_->generation == 0) state_->generation = 1;
        state_->request_pending = false;
        stop_system_request_timer(state_.get());
        advance_async_generation();
    }
    if (LeaveSelfPage) LeaveSelfPage();
}

void LvSettingSystemInfoPage3::create_ui(lv_obj_t *parent)
{
    if (!parent || !state_) return;
    using LayoutMetric = LvSettingSystemInfoPage3::LayoutMetric;
    ComponensObj = lv_obj_create(parent);
    if (!ComponensObj) return;
    configure_page(ComponensObj,
                   LvSettingSystemInfoPage3::metric(LvSettingSystemInfoPage3::LayoutMetric::ScreenW),
                   LvSettingSystemInfoPage3::metric(LvSettingSystemInfoPage3::LayoutMetric::ScreenH));
    DComponens::lvgl_bind_event(
        ComponensObj,
        LV_EVENT_KEY,
        nullptr,
        std::bind(&system_info_key_event, this, std::placeholders::_1));

    const bool network_table = uses_network_table(state_->kind);
    state_->title = create_label(
        ComponensObj,
        LvSettingSystemInfoPage3::metric(LayoutMetric::TextX),
        LvSettingSystemInfoPage3::metric(LayoutMetric::TitleY),
        LvSettingSystemInfoPage3::metric(LayoutMetric::TextW),
        "System",
        0x58A6FF,
        network_table ? settings_fonts::sans(16, LV_FREETYPE_FONT_STYLE_BOLD) : title_font());
    if (network_table) {
        state_->line_one = create_network_table_row(ComponensObj, 0, "IP");
        state_->line_two = create_network_table_row(ComponensObj, 1, "Gateway");
        state_->line_three = create_network_table_row(ComponensObj, 2, "MAC");
    } else {
        state_->line_one = create_label(ComponensObj,
                                        LvSettingSystemInfoPage3::metric(LayoutMetric::TextX),
                                        LvSettingSystemInfoPage3::metric(LayoutMetric::LineOneY),
                                        LvSettingSystemInfoPage3::metric(LayoutMetric::TextW),
                                        "", 0xECECEC, body_font());
        state_->line_two = create_label(ComponensObj,
                                        LvSettingSystemInfoPage3::metric(LayoutMetric::TextX),
                                        LvSettingSystemInfoPage3::metric(LayoutMetric::LineTwoY),
                                        LvSettingSystemInfoPage3::metric(LayoutMetric::TextW),
                                        "", 0xCCCCCC, body_font());
        state_->line_three = create_label(ComponensObj,
                                          LvSettingSystemInfoPage3::metric(LayoutMetric::TextX),
                                          LvSettingSystemInfoPage3::metric(LayoutMetric::LineThreeY),
                                          LvSettingSystemInfoPage3::metric(LayoutMetric::TextW),
                                          "", 0xAAAAAA, body_font());
    }
    state_->status_label = create_label(ComponensObj,
                                        LvSettingSystemInfoPage3::metric(LayoutMetric::TextX),
                                        network_table
                                            ? LvSettingSystemInfoPage3::metric(LayoutMetric::TableStatusY)
                                            : LvSettingSystemInfoPage3::metric(LayoutMetric::StatusY),
                                        LvSettingSystemInfoPage3::metric(LayoutMetric::TextW),
                                        "", 0xF0C850,
                                        network_table ? settings_fonts::sans(11) : body_font(),
                                        !network_table);
    state_->hint = create_label(ComponensObj,
                                LvSettingSystemInfoPage3::metric(LayoutMetric::TextX),
                                LvSettingSystemInfoPage3::metric(LayoutMetric::HintY),
                                LvSettingSystemInfoPage3::metric(LayoutMetric::TextW),
                                "", 0x46DC87,
                                network_table ? settings_fonts::sans(10, LV_FREETYPE_FONT_STYLE_BOLD)
                                              : hint_font());
    if (network_table) {
        if (state_->status_label) {
            lv_obj_set_height(
                state_->status_label,
                LvSettingSystemInfoPage3::metric(LayoutMetric::TableStatusH));
            lv_label_set_long_mode(state_->status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        }
        if (state_->hint) {
            lv_obj_set_height(
                state_->hint,
                LvSettingSystemInfoPage3::metric(LayoutMetric::TableHintH));
            lv_label_set_long_mode(state_->hint, LV_LABEL_LONG_CLIP);
        }
    }
    refresh_system_info(this);
}

enum class SoftwareDialogOption
{
    Apps,
    Launcher,
    NotNow,
};

struct LvSettingUpdatePage3::State
{
    settings_system::UpdateAction action = settings_system::UpdateAction::UpdateLauncher;
    uint64_t generation = 1;
    bool request_pending = false;
    bool poll_pending = false;
    bool update_pending = false;
    bool software_checking = false;
    bool launcher_update = false;
    bool apps_updating = false;
    bool cancelling = false;
    bool leaving = false;
    bool dialog_open = false;
    int dialog_selection = 0;
    std::string job_id;
    std::string backend_state;
    std::string status = "Press ENTER to check for updates";
    int progress = static_cast<int>(settings_system::UpdateStatusInfo::Progress::Unknown);
    settings_system::UpdatePhase phase = settings_system::UpdatePhase::Idle;
    std::chrono::steady_clock::time_point deadline;
    lv_timer_t *poll_timer = nullptr;
    lv_obj_t *title = nullptr;
    lv_obj_t *summary = nullptr;
    lv_obj_t *scroll_body = nullptr;
    lv_obj_t *status_label = nullptr;
    lv_obj_t *hint = nullptr;

    struct AppRow {
        lv_obj_t *name = nullptr;
        lv_obj_t *description = nullptr;
        std::string package;
        std::string short_name;
        std::string version = "--";
    };
    std::vector<AppRow> app_rows;
    settings_software::CheckResult check;
    settings_software::AppUpdateState::Phase app_update_phase =
        settings_software::AppUpdateState::Phase::Unknown;
    std::chrono::steady_clock::time_point app_update_started;
    bool scroll_animation_active = false;
    lv_obj_t *keyboard_root = nullptr;
    lv_event_dsc_t *keyboard_event_dsc = nullptr;

    lv_obj_t *dialog = nullptr;
    lv_obj_t *dialog_content = nullptr;
    lv_obj_t *dialog_message = nullptr;
    lv_obj_t *apps_button = nullptr;
    lv_obj_t *update_button = nullptr;
    lv_obj_t *skip_button = nullptr;
    lv_obj_t *ok_button = nullptr;
    lv_obj_t *progress_bar = nullptr;
    lv_obj_t *progress_label = nullptr;
    std::vector<SoftwareDialogOption> dialog_options;
};

namespace {

bool is_update_job_state(const std::string &state)
{
    const auto info = settings_system::parse_update_status(state);
    return state == "running" || !info.stage.empty() ||
           state.rfind("recovering:", 0) == 0 ||
           state == "cancelled" || state == "canceled" ||
           state == "timeout" || state == "timed-out" ||
           state.rfind("succeeded:", 0) == 0 || state.rfind("failed:", 0) == 0;
}

bool is_update_progress_state(const std::string &state)
{
    const auto info = settings_system::parse_update_status(state);
    return !info.terminal && (info.stage == "running" || info.stage == "checking" ||
           info.stage == "downloading" || info.stage == "verifying" ||
           info.stage == "repairing" || info.stage == "installing" ||
           info.stage == "restarting" || info.stage == "recovering");
}

int stage_progress(const settings_system::UpdateStatusInfo &info)
{
    if (info.progress != static_cast<int>(settings_system::UpdateStatusInfo::Progress::Unknown))
        return info.progress;
    if (info.stage == "checking") return 10;
    if (info.stage == "downloading") return 25;
    if (info.stage == "verifying") return 45;
    if (info.stage == "repairing") return 55;
    if (info.stage == "installing") return 75;
    if (info.stage == "recovering") return 85;
    if (info.stage == "restarting") return 95;
    return 0;
}

std::string progress_status(const settings_system::UpdateStatusInfo &info)
{
    if (info.stage == "checking") return "Checking for updates...";
    if (info.stage == "downloading") return "Downloading APPLaunch update...";
    if (info.stage == "verifying") return "Verifying APPLaunch update...";
    if (info.stage == "repairing") return "Repairing package state...";
    if (info.stage == "installing") return "Installing APPLaunch update...";
    if (info.stage == "restarting") return "Restarting APPLaunch...";
    if (info.stage == "recovering") return "Restoring previous APPLaunch...";
    return "Working...";
}

template <typename StateT>
void clear_dialog_state(StateT *state)
{
    if (!state) return;
    state->dialog = nullptr;
    state->dialog_content = nullptr;
    state->dialog_message = nullptr;
    state->apps_button = nullptr;
    state->update_button = nullptr;
    state->skip_button = nullptr;
    state->ok_button = nullptr;
    state->progress_bar = nullptr;
    state->progress_label = nullptr;
    state->dialog_options.clear();
}

void set_label_color(lv_obj_t *label, uint32_t color)
{
    if (!label) return;
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
}

void keep_update_page_focus(LvSettingUpdatePage3 *page)
{
    if (!page || !page->Get()) return;
    lv_group_t *group = lv_obj_get_group(page->Get());
    if (group) lv_group_focus_obj(page->Get());
}

void remove_dialog_controls_from_group(lv_obj_t *dialog)
{
    if (!dialog) return;
    lv_obj_t *footer = lv_msgbox_get_footer(dialog);
    if (!footer) return;
    const uint32_t child_count = lv_obj_get_child_count(footer);
    for (uint32_t index = 0; index < child_count; ++index) {
        lv_obj_t *button = lv_obj_get_child(footer, index);
        if (button && lv_obj_get_group(button)) lv_group_remove_obj(button);
    }
}

void close_dialog(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    lv_obj_t *dialog = state->dialog;
    remove_dialog_controls_from_group(dialog);
    clear_dialog_state(state);
    state->dialog_open = false;
    if (dialog) lv_obj_delete(dialog);
    keep_update_page_focus(page);
}

void style_dialog(lv_obj_t *dialog)
{
    if (!dialog) return;
    lv_obj_set_size(dialog,
                    LvSettingUpdatePage3::metric(LvSettingUpdatePage3::LayoutMetric::DialogW),
                    LvSettingUpdatePage3::metric(LvSettingUpdatePage3::LayoutMetric::DialogH));
    lv_obj_center(dialog);
    lv_obj_set_style_radius(
        dialog, LvSettingUpdatePage3::metric(LvSettingUpdatePage3::LayoutMetric::DialogRadius), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        dialog, LvSettingUpdatePage3::metric(LvSettingUpdatePage3::LayoutMetric::DialogBorderWidth), LV_PART_MAIN);
    lv_obj_set_style_border_color(dialog, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0x171717), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dialog, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
}

void style_dialog_layout(lv_obj_t *dialog)
{
    if (!dialog) return;

    lv_obj_t *header = lv_msgbox_get_header(dialog);
    if (header) {
        lv_obj_set_height(header, 30);
        lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(header, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_right(header, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_top(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(header, 0, LV_PART_MAIN);
        lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    }

    lv_obj_t *content = lv_msgbox_get_content(dialog);
    if (content) {
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(content, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_right(content, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_top(content, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(content, 0, LV_PART_MAIN);
    }

    lv_obj_t *footer = lv_msgbox_get_footer(dialog);
    if (footer) {
        lv_obj_set_height(footer, 30);
        lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(footer, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(footer, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_right(footer, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_top(footer, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(footer, 0, LV_PART_MAIN);
        lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    }

    if (header) {
        lv_obj_t *title = lv_msgbox_get_title(dialog);
        if (title) {
            lv_obj_set_style_text_font(title, settings_fonts::sans(13, LV_FREETYPE_FONT_STYLE_BOLD),
                                       LV_PART_MAIN);
            lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        }
    }
    if (footer) {
        const uint32_t child_count = lv_obj_get_child_count(footer);
        for (uint32_t index = 0; index < child_count; ++index) {
            lv_obj_t *button = lv_obj_get_child(footer, index);
            if (!button) continue;
            lv_obj_set_height(button, 26);
            lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
            lv_obj_t *label = lv_obj_get_child(button, 0);
            if (label)
                lv_obj_set_style_text_font(label, settings_fonts::sans(12), LV_PART_MAIN);
        }
    }

    // The footer buttons are group-capable LVGL objects.  This page owns the
    // modal state machine, so keep navigation on the page root instead of
    // allowing the default input group to focus a button directly.
    remove_dialog_controls_from_group(dialog);
}

template <typename StateT>
void render_dialog_selection(StateT *state)
{
    if (!state) return;
    const auto style_button = [](lv_obj_t *button, bool selected) {
        if (!button) return;
        lv_obj_set_style_bg_color(
            button, lv_color_hex(selected ? 0x2878C8 : 0x333333), LV_PART_MAIN);
        lv_obj_set_style_border_width(button, selected ? 2 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(button, lv_color_hex(0x8FCBFF), LV_PART_MAIN);
    };
    for (std::size_t index = 0; index < state->dialog_options.size(); ++index) {
        lv_obj_t *button = nullptr;
        switch (state->dialog_options[index]) {
        case SoftwareDialogOption::Apps:
            button = state->apps_button;
            break;
        case SoftwareDialogOption::Launcher:
            button = state->update_button;
            break;
        case SoftwareDialogOption::NotNow:
            button = state->skip_button;
            break;
        }
        style_button(button, static_cast<int>(index) == state->dialog_selection);
    }
}

void render_update_page(LvSettingUpdatePage3 *page);

void scroll_body_by(LvSettingUpdatePage3 *page, int dy)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    if (!state->scroll_body || state->scroll_animation_active) return;
    lv_obj_scroll_by_bounded(state->scroll_body, 0, dy, LV_ANIM_ON);
}

void scroll_dialog_by(LvSettingUpdatePage3 *page, int dy)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    if (!state->dialog_content) return;
    lv_obj_scroll_by_bounded(state->dialog_content, 0, dy, LV_ANIM_ON);
}

void software_scroll_event_cb(lv_event_t *event)
{
    if (!event) return;
    auto *page = static_cast<LvSettingUpdatePage3 *>(lv_event_get_user_data(event));
    if (!page || !page->state_) return;
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_SCROLL_BEGIN)
        page->state_->scroll_animation_active = true;
    else if (code == LV_EVENT_SCROLL_END)
        page->state_->scroll_animation_active = false;
}

void schedule_job_cancel(std::string job_id)
{
    if (job_id.empty()) return;
    request_osinfo(
        {"UpdateJobCancel", std::move(job_id)}, [](int, std::string) {});
}

void schedule_app_update_cancel(LvSettingUpdatePage3 *page)
{
    start_process_request(
        page,
        {"systemctl", "stop", "--no-block", "applaunch-apps-updater.service"},
        [](int, std::string) {}, {});
}

template <typename StateT>
void stop_update_poll(StateT *state)
{
    if (!state) return;
    stop_timer(state->poll_timer);
    state->poll_pending = false;
}

void apply_update_marks(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    for (auto &row : state->app_rows) {
        const settings_software::UpdateInfo *found = nullptr;
        for (const auto &update : state->check.updates) {
            if (update.package == row.package) {
                found = &update;
                break;
            }
        }
        if (found && settings_software::update_changes_version(*found)) {
            const std::string installed = found->installed.empty() ? row.version : found->installed;
            set_label(row.name, row.short_name + " " + installed + " -> " + found->latest);
            set_label_color(row.name, 0xF0C850);
        } else {
            set_label(row.name, row.short_name + " " + row.version);
            set_label_color(row.name, 0xF2F2F2);
        }
    }
}

void reset_update_marks(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    page->state_->check = settings_software::CheckResult{};
    apply_update_marks(page);
}

void render_update_page(LvSettingUpdatePage3 *page)
{
    if (!page || !page->Get() || !page->state_) return;
    auto *state = page->state_.get();
    set_label(state->status_label, state->status);
    if (state->progress_label) set_label(state->progress_label, state->status);
    if (state->progress_bar)
        lv_bar_set_value(state->progress_bar, std::max(0, state->progress), LV_ANIM_OFF);

    std::string summary = "APPLaunch " + std::string(launcher_version());
    uint32_t summary_color = 0xECECEC;
    if (state->check.launcher_available) {
        const std::string installed = state->check.launcher_installed.empty()
            ? std::string(launcher_version()) : state->check.launcher_installed;
        summary = "APPLaunch " + installed + " -> " + state->check.launcher_version;
        summary_color = 0xF0C850;
    }
    set_label(state->summary, summary);
    set_label_color(state->summary, summary_color);

    if (state->hint) {
        const char *hint = "ENTER: check   ESC: back";
        if (state->dialog_open)
            hint = "";
        else if (state->request_pending || state->update_pending)
            hint = "ESC: cancel";
        lv_label_set_text(state->hint, hint);
    }
}

void show_updates_dialog(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_ || !page->Get()) return;
    auto *state = page->state_.get();
    std::vector<std::string> lines;
    if (state->check.launcher_available) {
        const std::string installed = state->check.launcher_installed.empty()
            ? std::string(launcher_version()) : state->check.launcher_installed;
        lines.push_back(
            settings_software::launcher_update_line(installed, state->check.launcher_version));
    }
    for (const auto &update : state->check.updates) {
        if (settings_software::update_changes_version(update))
            lines.push_back(settings_software::update_line(update));
    }
    if (state->check.failed > 0)
        lines.push_back(std::to_string(state->check.failed) + " package check(s) failed");
    if (state->check.launcher_failed)
        lines.push_back("APPLaunch update check failed");
    if (lines.empty()) return;

    std::string message;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index != 0) message.push_back('\n');
        message += lines[index];
    }

    close_dialog(page);
    state->dialog = lv_msgbox_create(page->Get());
    if (!state->dialog) {
        state->status = "Unable to show update results";
        return;
    }
    style_dialog(state->dialog);
    lv_obj_t *title = lv_msgbox_add_title(state->dialog, "Updates available");
    if (title) lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);

    lv_obj_t *content = lv_msgbox_get_content(state->dialog);
    style_dialog_layout(state->dialog);
    if (content) {
        lv_obj_set_height(
            content, LvSettingUpdatePage3::metric(LvSettingUpdatePage3::LayoutMetric::DialogContentH));
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(content, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(content, 2, LV_PART_MAIN);
        state->dialog_content = content;
        state->dialog_message = lv_label_create(content);
        if (state->dialog_message) {
            lv_obj_set_width(state->dialog_message, lv_pct(100));
            lv_label_set_long_mode(state->dialog_message, LV_LABEL_LONG_WRAP);
            lv_label_set_text(state->dialog_message, message.c_str());
            lv_obj_set_style_text_color(state->dialog_message, lv_color_hex(0xECECEC), LV_PART_MAIN);
            lv_obj_set_style_text_font(
                state->dialog_message, settings_fonts::sans(12), LV_PART_MAIN);
        }
    }

    state->dialog_options.clear();
    if (!state->check.updates.empty()) {
        state->apps_button =
            lv_msgbox_add_footer_button(state->dialog, "Update apps");
        state->dialog_options.push_back(SoftwareDialogOption::Apps);
    }
    if (state->check.launcher_available) {
        state->update_button =
            lv_msgbox_add_footer_button(state->dialog, "Update APPLaunch");
        state->dialog_options.push_back(SoftwareDialogOption::Launcher);
    }
    state->skip_button = lv_msgbox_add_footer_button(state->dialog, "Not now");
    state->dialog_options.push_back(SoftwareDialogOption::NotNow);
    if (state->dialog_options.size() <= 2) {
        if (state->apps_button) lv_obj_set_flex_grow(state->apps_button, 1);
        if (state->update_button) lv_obj_set_flex_grow(state->update_button, 1);
        if (state->skip_button) lv_obj_set_flex_grow(state->skip_button, 1);
    }
    style_dialog_layout(state->dialog);
    lv_obj_update_layout(state->dialog);
    state->dialog_open = true;
    state->dialog_selection = 0;
    keep_update_page_focus(page);
    render_dialog_selection(state);
    render_update_page(page);
}

void show_update_progress(LvSettingUpdatePage3 *page,
                          const char *title_text = "Updating APPLaunch")
{
    if (!page || !page->state_ || !page->Get()) return;
    auto *state = page->state_.get();
    close_dialog(page);
    state->dialog = lv_msgbox_create(page->Get());
    if (!state->dialog) return;
    style_dialog(state->dialog);
    lv_obj_t *title = lv_msgbox_add_title(state->dialog, title_text);
    if (title) lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_t *content = lv_msgbox_get_content(state->dialog);
    if (!content) return;
    style_dialog_layout(state->dialog);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(
        content, LvSettingUpdatePage3::metric(LvSettingUpdatePage3::LayoutMetric::ContentPadRow), LV_PART_MAIN);
    state->progress_label = lv_label_create(content);
    if (state->progress_label) {
        lv_obj_set_width(state->progress_label, lv_pct(100));
        lv_label_set_long_mode(state->progress_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_label_set_text(state->progress_label, state->status.c_str());
    }
    state->progress_bar = lv_bar_create(content);
    if (state->progress_bar) {
        lv_obj_set_size(
            state->progress_bar,
            lv_pct(100),
            LvSettingUpdatePage3::metric(LvSettingUpdatePage3::LayoutMetric::ProgressBarH));
        lv_bar_set_range(state->progress_bar, 0, 100);
        lv_bar_set_value(state->progress_bar, std::max(0, state->progress), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(
            state->progress_bar, lv_color_hex(0x2878C8), LV_PART_INDICATOR);
    }
    lv_obj_update_layout(state->dialog);
    state->dialog_open = true;
    keep_update_page_focus(page);
    render_update_page(page);
}

std::string update_failure_status(settings_system::UpdateAction action,
                                  int code,
                                  const std::string &state)
{
    if (code != 0 && (state.empty() || !is_update_job_state(state)))
        return settings_system::backend_error_label(code, state);
    return settings_system::update_job_label(action, code, state);
}

void finish_update(LvSettingUpdatePage3 *page,
                   settings_system::UpdatePhase phase,
                   int code,
                   std::string backend_state,
                   bool cancel_backend)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    const bool was_launcher_update = state->launcher_update;
    const std::string job_id = std::move(state->job_id);
    state->job_id.clear();
    stop_update_poll(state);
    state->request_pending = false;
    state->update_pending = false;
    state->software_checking = false;
    state->launcher_update = false;
    state->apps_updating = false;
    state->cancelling = false;
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    page->advance_async_generation();
    state->backend_state = std::move(backend_state);
    state->phase = phase;
    if (phase == settings_system::UpdatePhase::Succeeded) {
        state->progress = 100;
        if (was_launcher_update) {
            state->check.launcher_available = false;
            state->check.launcher_failed = false;
        }
        state->status = settings_system::update_job_label(state->action, code, state->backend_state);
    } else if (phase == settings_system::UpdatePhase::Failed) {
        state->status = update_failure_status(state->action, code, state->backend_state);
    } else {
        state->status = settings_system::update_phase_label(
            state->action, phase, code, state->backend_state);
    }
    if (phase != settings_system::UpdatePhase::Running) close_dialog(page);
    render_update_page(page);
    if (cancel_backend && !job_id.empty()) schedule_job_cancel(job_id);
}

void finish_software_check(LvSettingUpdatePage3 *page,
                           int code,
                           std::string payload,
                           bool cancel_backend)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    const std::string job_id = std::move(state->job_id);
    state->job_id.clear();
    stop_update_poll(state);
    state->request_pending = false;
    state->update_pending = false;
    state->software_checking = false;
    state->launcher_update = false;
    state->apps_updating = false;
    state->cancelling = false;
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    page->advance_async_generation();
    state->backend_state = payload;

    if (code == -ECANCELED || payload == "cancelled" || payload == "canceled") {
        state->phase = settings_system::UpdatePhase::Cancelled;
        state->status = "Update check cancelled";
        render_update_page(page);
        return;
    }

    settings_software::CheckResult result;
    if (code != 0 || !settings_software::parse_check_result(payload, result)) {
        state->phase = code == -ETIMEDOUT ? settings_system::UpdatePhase::TimedOut
                                          : settings_system::UpdatePhase::Failed;
        state->status = code == -ETIMEDOUT ? "Update check timed out"
                                           : "Unable to check for updates";
        render_update_page(page);
        if (cancel_backend && !job_id.empty()) schedule_job_cancel(job_id);
        return;
    }

    state->check = std::move(result);
    state->check.updates.erase(
        std::remove_if(state->check.updates.begin(), state->check.updates.end(),
                       [](const settings_software::UpdateInfo &update) {
                           return !settings_software::update_changes_version(update);
                       }),
        state->check.updates.end());
    apply_update_marks(page);
    state->phase = settings_system::UpdatePhase::Succeeded;
    state->progress = 100;
    const int update_count = static_cast<int>(state->check.updates.size()) +
                             (state->check.launcher_available ? 1 : 0);
    if (update_count == 0) {
        if (state->check.launcher_failed && state->check.checked > 0 &&
            state->check.failed >= state->check.checked) {
            state->status = "Unable to check for updates";
        } else if (state->check.failed > 0 || state->check.launcher_failed) {
            state->status = "Some checks failed; no updates found";
        } else {
            state->status = "All software is up to date";
        }
        render_update_page(page);
        if (cancel_backend && !job_id.empty()) schedule_job_cancel(job_id);
        return;
    }

    state->status = std::to_string(update_count) +
                    (update_count == 1 ? " update available" : " updates available");
    render_update_page(page);
    show_updates_dialog(page);
    if (cancel_backend && !job_id.empty()) schedule_job_cancel(job_id);
}

void refresh_installed_rows(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    const std::vector<settings_software::InstalledApp> apps =
        settings_software::read_installed_apps();
    for (auto &row : state->app_rows) {
        const settings_software::InstalledApp *found = nullptr;
        for (const auto &app : apps) {
            if (app.package == row.package) {
                found = &app;
                break;
            }
        }
        if (!found) continue;
        row.version = found->version;
        if (row.description)
            lv_label_set_text(row.description, found->description.c_str());
    }
    state->check.updates.clear();
    apply_update_marks(page);
}

void finish_apps_update(LvSettingUpdatePage3 *page,
                        int code,
                        std::string payload,
                        bool cancel_backend)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    stop_update_poll(state);
    state->request_pending = false;
    state->update_pending = false;
    state->software_checking = false;
    state->launcher_update = false;
    state->apps_updating = false;
    state->cancelling = false;
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    page->advance_async_generation();
    close_dialog(page);
    refresh_installed_rows(page);

    settings_software::AppUpdateState result;
    const bool parsed = settings_software::parse_app_update_status(payload, result);
    if (parsed) state->app_update_phase = result.phase;
    if (code == -ETIMEDOUT) {
        state->phase = settings_system::UpdatePhase::TimedOut;
        state->status = "App update timed out";
    } else if (!parsed) {
        state->phase = settings_system::UpdatePhase::Failed;
        state->status = "Unable to update apps";
    } else if (result.phase == settings_software::AppUpdateState::Phase::Succeeded) {
        state->phase = settings_system::UpdatePhase::Succeeded;
        state->progress = 100;
        if (result.updated_count == 0 && result.failed_count == 0) {
            state->status = "Apps are up to date";
        } else if (result.failed_count > 0) {
            state->status = std::to_string(result.updated_count) +
                            " app(s) updated, " +
                            std::to_string(result.failed_count) + " failed";
        } else {
            state->status = std::to_string(result.updated_count) + " app(s) updated";
        }
    } else if (result.phase == settings_software::AppUpdateState::Phase::Cancelled ||
               result.phase == settings_software::AppUpdateState::Phase::Cancelling) {
        state->phase = settings_system::UpdatePhase::Cancelled;
        state->status = "App updates cancelled";
    } else if (result.phase == settings_software::AppUpdateState::Phase::Failed) {
        state->phase = settings_system::UpdatePhase::Failed;
        state->status = result.detail.empty()
            ? "App update failed"
            : "App update failed: " + result.detail;
    } else {
        state->phase = settings_system::UpdatePhase::Failed;
        state->status = "App update did not finish";
    }
    if (cancel_backend) schedule_app_update_cancel(page);
    render_update_page(page);
}

void poll_apps_update(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    if (!state->request_pending && !state->update_pending) return;
    if (std::chrono::steady_clock::now() >= state->deadline) {
        finish_apps_update(page, -ETIMEDOUT, "timeout", true);
        return;
    }
    if (!state->update_pending) return;
    state->poll_pending = false;

    const std::string payload = read_app_update_status();
    if (payload.empty()) {
        state->status = "Starting app updates...";
        render_update_page(page);
        return;
    }

    settings_software::AppUpdateState result;
    if (!settings_software::parse_app_update_status(payload, result)) {
        finish_apps_update(page, -1, payload, false);
        return;
    }
    state->app_update_phase = result.phase;
    if (result.phase == settings_software::AppUpdateState::Phase::Checking ||
        result.phase == settings_software::AppUpdateState::Phase::Downloading ||
        result.phase == settings_software::AppUpdateState::Phase::Installing ||
        result.phase == settings_software::AppUpdateState::Phase::Cancelling) {
        const int total = std::max(1, result.total);
        const int index = std::max(1, std::min(result.index, total));
        state->progress = (index - 1) * 100 / total;
        if (result.phase == settings_software::AppUpdateState::Phase::Checking) {
            state->status = "Checking app versions... (" +
                            std::to_string(index) + "/" +
                            std::to_string(total) + ")";
        } else if (result.phase ==
                   settings_software::AppUpdateState::Phase::Downloading) {
            state->status = "Downloading " +
                            settings_software::short_name(result.package) +
                            " (" + std::to_string(index) + "/" +
                            std::to_string(total) + ")...";
        } else if (result.phase ==
                   settings_software::AppUpdateState::Phase::Installing) {
            state->status = "Installing " +
                            settings_software::short_name(result.package) +
                            " (" + std::to_string(index) + "/" +
                            std::to_string(total) + ")...";
        } else {
            state->status = "Cancelling app updates...";
        }
        if (state->cancelling)
            state->status = "Cancelling app updates...";
        render_update_page(page);
        return;
    }

    if (std::chrono::steady_clock::now() - state->app_update_started <
        std::chrono::seconds(1)) {
        state->status = "Starting app updates...";
        render_update_page(page);
        return;
    }
    finish_apps_update(page, 0, payload, false);
}

void poll_update(LvSettingUpdatePage3 *page);

void update_poll_timer_cb(lv_timer_t *timer) noexcept
{
    auto *page = timer ? static_cast<LvSettingUpdatePage3 *>(lv_timer_get_user_data(timer)) : nullptr;
    if (!page || !page->state_ || timer != page->state_->poll_timer) return;
    poll_update(page);
}

void poll_update(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    if (!state->request_pending && !state->update_pending) return;
    if (state->apps_updating) {
        poll_apps_update(page);
        return;
    }
    if (state->software_checking) {
        if (std::chrono::steady_clock::now() >= state->deadline) {
            page->async_tasks().cancel();
            settings_http::wake();
            finish_software_check(page, -ETIMEDOUT, "timeout", false);
        }
        return;
    }
    if (std::chrono::steady_clock::now() >= state->deadline) {
        finish_update(page, settings_system::UpdatePhase::TimedOut, -ETIMEDOUT, "timeout", true);
        return;
    }
    if (!state->update_pending || state->poll_pending || state->job_id.empty()) return;

    state->poll_pending = true;
    const uint64_t request_generation = state->generation;
    const std::string job_id = state->job_id;
    if (!start_osinfo_request(
            page,
            {"UpdateJobStatus", job_id},
            [page, request_generation](int code, std::string payload) {
                auto *state = page ? page->state_.get() : nullptr;
                if (!state || request_generation != state->generation || !page->Get()) return;
                state->poll_pending = false;
                if (!state->update_pending) return;
                const auto info = settings_system::parse_update_status(payload);
                if (code == 0 && is_update_progress_state(payload)) {
                    state->phase = settings_system::UpdatePhase::Running;
                    state->progress = stage_progress(info);
                    if (state->software_checking) {
                        state->status = state->progress >= 85
                            ? "Checking APPLaunch update..."
                            : "Checking for updates... " +
                                  std::to_string(std::max(0, state->progress)) + "%";
                    } else if (state->cancelling) {
                        state->status = "Cancelling update...";
                    } else {
                        state->status = progress_status(info);
                    }
                    render_update_page(page);
                    return;
                }
                if (payload == "cancelled" || payload == "canceled" ||
                    payload.rfind("failed:cancelled:", 0) == 0 ||
                    payload.rfind("failed:canceled:", 0) == 0) {
                    if (state->software_checking)
                        finish_software_check(page, code, std::move(payload), false);
                    else
                        finish_update(page, settings_system::UpdatePhase::Cancelled, code,
                                      std::move(payload), false);
                    return;
                }
                if (state->software_checking) {
                    finish_software_check(page, code, std::move(payload), false);
                    return;
                }
                if (code == 0 && payload.rfind("succeeded:", 0) == 0) {
                    state->progress = 100;
                    finish_update(page, settings_system::UpdatePhase::Succeeded, code,
                                  std::move(payload), false);
                    return;
                }
                finish_update(page, settings_system::UpdatePhase::Failed, code,
                              std::move(payload), false);
            })) {
        state->poll_pending = false;
        if (state->software_checking)
            finish_software_check(page, -1, "status request could not be scheduled", true);
        else
            finish_update(page, settings_system::UpdatePhase::Failed, -1,
                          "status request could not be scheduled", true);
    }
}

void start_launcher_update(LvSettingUpdatePage3 *page);

void start_software_check(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    if (state->request_pending || state->update_pending || state->leaving) return;

    close_dialog(page);
    reset_update_marks(page);
    page->advance_async_generation();
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    state->request_pending = true;
    state->software_checking = true;
    state->launcher_update = false;
    state->apps_updating = false;
    state->cancelling = false;
    state->progress = 0;
    state->phase = settings_system::UpdatePhase::Starting;
    state->status = "Checking for updates...";
    state->deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);
    stop_update_poll(state);
    state->poll_timer = lv_timer_create(
        update_poll_timer_cb, static_cast<uint32_t>(TimerInterval::UpdatePollMs), page);
    if (!state->poll_timer) {
        state->request_pending = false;
        state->software_checking = false;
        finish_software_check(page, -1, "status timer unavailable", false);
        return;
    }
    render_update_page(page);

    if (!page->ensure_async_dispatch()) {
        state->request_pending = false;
        finish_software_check(page, -1, "async dispatch unavailable", false);
        return;
    }
    const auto token = page->async_token();
    if (!token.valid()) {
        state->request_pending = false;
        finish_software_check(page, -1, "async dispatch unavailable", false);
        return;
    }

    std::vector<settings_software::AppSpec> specs;
    for (const auto &app : settings_software::app_definitions()) {
        specs.push_back({app.package, app.repo, app.asset_prefix,
                         settings_software::kAssetSuffix});
    }

    if (!page->async_tasks().start(
            [page, token, specs](auto stop) {
                std::atomic<bool> fallback{false};
                std::atomic<bool> &cancel = stop ? *stop : fallback;
                const auto report_progress = [page, token](int value) {
                    if (!token.valid()) return;
                    SettingsAsync::Dispatch::enqueue_from_callback(
                        token, [page, value]() {
                            if (!page->state_ || !page->state_->software_checking)
                                return;
                            page->state_->progress = value;
                            page->state_->status = value >= 85
                                ? "Checking APPLaunch update..."
                                : "Checking for updates... " +
                                      std::to_string(std::max(0, value)) + "%";
                            render_update_page(page);
                        });
                };

                CurlCache cache;
                prefetch_software_http(specs, cancel, report_progress, cache);
                if (cancel.load(std::memory_order_acquire)) return;

                auto capture = [&cancel, &cache](
                                   const std::vector<std::string> &arguments,
                                   std::string &output) {
                    if (cancel.load(std::memory_order_acquire)) return -ECANCELED;
                    if (!arguments.empty() && arguments.front() == "curl") {
                        const std::string &url = arguments.back();
                        const auto found = cache.find(url);
                        if (found != cache.end()) {
                            output = found->second.output;
                            return found->second.code;
                        }
                    }
                    return capture_process(arguments, output);
                };
                const settings_software::Result result =
                    settings_software::check_software(
                        specs, capture, launcher_release_url(), cancel,
                        [report_progress](int value) {
                            report_progress(70 + value * 30 / 100);
                        });
                if (!token.valid()) return;
                if (result.code == -ECANCELED) return;
                SettingsAsync::Dispatch::enqueue_from_callback(
                    token, [page, result]() {
                        if (!page->state_ || !page->state_->software_checking)
                            return;
                        finish_software_check(page, result.code, result.stage,
                                              false);
                    });
            })) {
        state->request_pending = false;
        finish_software_check(page, -1, "check request could not be scheduled", false);
    }
}

void start_launcher_update(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    if (state->request_pending || state->update_pending || state->leaving) return;

    page->advance_async_generation();
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    const uint64_t request_generation = state->generation;
    state->request_pending = true;
    state->software_checking = false;
    state->launcher_update = true;
    state->apps_updating = false;
    state->cancelling = false;
    state->progress = 0;
    state->phase = settings_system::UpdatePhase::Starting;
    state->status = "Starting APPLaunch update...";
    state->deadline = std::chrono::steady_clock::now() + kUpdateTimeout;
    stop_update_poll(state);
    state->poll_timer = lv_timer_create(
        update_poll_timer_cb, static_cast<uint32_t>(TimerInterval::UpdatePollMs), page);
    if (!state->poll_timer) {
        state->request_pending = false;
        finish_update(page, settings_system::UpdatePhase::Failed, -1,
                      "status timer unavailable", false);
        return;
    }
    show_update_progress(page);
    render_update_page(page);

    const char *command = settings_system::update_request(state->action);
    if (!start_osinfo_request(
            page,
            {command},
            [page, request_generation](int code, std::string payload) {
                auto *state = page ? page->state_.get() : nullptr;
                if (!state || request_generation != state->generation || !page->Get()) return;
                state->request_pending = false;
                if (code != 0 || payload.empty()) {
                    finish_update(page, settings_system::UpdatePhase::Failed,
                                  code == 0 ? -1 : code,
                                  payload.empty() ? "update service returned no job id" :
                                                     std::move(payload),
                                  false);
                    return;
                }
                state->job_id = std::move(payload);
                state->update_pending = true;
                state->phase = settings_system::UpdatePhase::Running;
                if (state->cancelling) {
                    schedule_job_cancel(state->job_id);
                    state->status = "Cancellation requested...";
                } else {
                    state->status = "Updating APPLaunch...";
                }
                render_update_page(page);
                lv_timer_ready(state->poll_timer);
            },
            [](int, std::string payload) {
                if (!payload.empty()) schedule_job_cancel(std::move(payload));
            })) {
        state->request_pending = false;
        finish_update(page, settings_system::UpdatePhase::Failed, -1,
                      "update request could not be scheduled", false);
    }
}

void start_apps_update(LvSettingUpdatePage3 *page)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    if (state->request_pending || state->update_pending || state->leaving) return;

    close_dialog(page);
    page->advance_async_generation();
    ++state->generation;
    if (state->generation == 0) state->generation = 1;
    const uint64_t request_generation = state->generation;
    state->request_pending = true;
    state->software_checking = false;
    state->launcher_update = false;
    state->apps_updating = true;
    state->cancelling = false;
    state->app_update_phase = settings_software::AppUpdateState::Phase::Unknown;
    state->app_update_started = std::chrono::steady_clock::now();
    state->progress = 0;
    state->phase = settings_system::UpdatePhase::Starting;
    state->status = "Starting app updates...";
    state->deadline = std::chrono::steady_clock::now() + std::chrono::minutes(30);
    stop_update_poll(state);
    state->poll_timer = lv_timer_create(
        update_poll_timer_cb, static_cast<uint32_t>(TimerInterval::UpdatePollMs), page);
    if (!state->poll_timer) {
        state->request_pending = false;
        finish_apps_update(page, -1, "status timer unavailable", false);
        return;
    }
    show_update_progress(page, "Updating apps");
    render_update_page(page);

    if (!start_process_request(
            page,
            {"systemctl", "start", "--no-block",
             "applaunch-apps-updater.service"},
            [page, request_generation](int code, std::string) {
                auto *state = page ? page->state_.get() : nullptr;
                if (!state || request_generation != state->generation || !page->Get()) return;
                state->request_pending = false;
                if (code != 0) {
                    finish_apps_update(page, code, "unable to start app updater", false);
                    return;
                }
                state->update_pending = true;
                state->phase = settings_system::UpdatePhase::Running;
                state->status = "Updating apps...";
                render_update_page(page);
                lv_timer_ready(state->poll_timer);
            },
            [](int, std::string) {})) {
        state->request_pending = false;
        finish_apps_update(page, -1, "update request could not be scheduled", false);
    }
}

void cancel_update(LvSettingUpdatePage3 *page, bool leave_page)
{
    if (!page || !page->state_ || page->state_->leaving) return;
    auto *state = page->state_.get();

    if (state->apps_updating) {
        if (leave_page) {
            // The root updater service keeps running after the page closes.
            state->leaving = true;
            if (page->LeaveSelfPage) page->LeaveSelfPage();
            return;
        }
        if (state->app_update_phase ==
            settings_software::AppUpdateState::Phase::Installing) {
            state->status = "Installing updates; please wait";
            render_update_page(page);
            return;
        }
        if (!state->cancelling) {
            state->cancelling = true;
            schedule_app_update_cancel(page);
            state->status = "Cancelling app updates...";
            render_update_page(page);
        }
        return;
    }

    if (state->software_checking) {
        page->async_tasks().cancel();
        settings_http::wake();
        stop_update_poll(state);
        state->request_pending = false;
        state->update_pending = false;
        state->software_checking = false;
        state->cancelling = false;
        ++state->generation;
        if (state->generation == 0) state->generation = 1;
        page->advance_async_generation();
        state->phase = settings_system::UpdatePhase::Cancelled;
        state->status = "Update check cancelled";
        render_update_page(page);
        if (leave_page) {
            state->leaving = true;
            if (page->LeaveSelfPage) page->LeaveSelfPage();
        }
        return;
    }

    const bool active = state->request_pending || state->update_pending;
    if (active) {
        if (leave_page) {
            if (!state->job_id.empty()) schedule_job_cancel(state->job_id);
        } else if (!state->cancelling) {
            state->cancelling = true;
            if (state->job_id.empty()) {
                state->status = "Waiting for update job before cancelling...";
            } else {
                state->status = "Cancelling update...";
                const uint64_t request_generation = state->generation;
                const std::string job_id = state->job_id;
                if (!start_osinfo_request(
                        page,
                        {"UpdateJobCancel", job_id},
                        [page, request_generation](int code, std::string) {
                            auto *cancel_state = page ? page->state_.get() : nullptr;
                            if (!cancel_state || request_generation != cancel_state->generation ||
                                !page->Get())
                                return;
                            if (code != 0) {
                                cancel_state->cancelling = false;
                                cancel_state->status = "Unable to cancel; update is still running";
                            } else {
                                cancel_state->status = "Cancellation requested...";
                            }
                            render_update_page(page);
                        })) {
                    state->cancelling = false;
                    state->status = "Unable to send cancellation request";
                }
            }
            render_update_page(page);
        }
    } else {
        ++state->generation;
        if (state->generation == 0) state->generation = 1;
        page->advance_async_generation();
    }
    if (leave_page) {
        state->leaving = true;
        if (page->LeaveSelfPage) page->LeaveSelfPage();
    }
}

void handle_dialog_key(LvSettingUpdatePage3 *page, uint32_t key)
{
    if (!page || !page->state_) return;
    auto *state = page->state_.get();
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        scroll_dialog_by(page, LvSettingUpdatePage3::metric(
                                   LvSettingUpdatePage3::LayoutMetric::ScrollStep));
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        scroll_dialog_by(page, -LvSettingUpdatePage3::metric(
                                    LvSettingUpdatePage3::LayoutMetric::ScrollStep));
        return;
    }

    const std::size_t option_count = state->dialog_options.size();
    if (option_count > 1 && (key == LV_KEY_LEFT || key == LV_KEY_RIGHT)) {
        const int delta = key == LV_KEY_LEFT ? -1 : 1;
        state->dialog_selection =
            (state->dialog_selection + delta + static_cast<int>(option_count)) %
            static_cast<int>(option_count);
        render_dialog_selection(state);
        return;
    }
    if (key == LV_KEY_ENTER) {
        const SoftwareDialogOption option = option_count == 0
            ? SoftwareDialogOption::NotNow
            : state->dialog_options[static_cast<std::size_t>(state->dialog_selection)];
        close_dialog(page);
        switch (option) {
        case SoftwareDialogOption::Apps:
            state->status = "Starting app updates...";
            render_update_page(page);
            start_apps_update(page);
            break;
        case SoftwareDialogOption::Launcher:
            state->status = "Starting APPLaunch update...";
            render_update_page(page);
            start_launcher_update(page);
            break;
        case SoftwareDialogOption::NotNow:
            state->status = "Update skipped";
            render_update_page(page);
            break;
        }
        return;
    }
    if (key == LV_KEY_ESC) {
        close_dialog(page);
        render_update_page(page);
    }
}

void software_keyboard_event_cb(lv_event_t *event)
{
    if (!event) return;
    auto *page = static_cast<LvSettingUpdatePage3 *>(lv_event_get_user_data(event));
    auto *item = static_cast<const key_item *>(lv_event_get_param(event));
    if (!page || !page->state_ || !item || item->key_state != KBD_KEY_REPEATED) return;

    uint32_t key = 0;
    switch (item->key_code) {
    case KEY_F:
    case KEY_UP:
        key = KEY_UP;
        break;
    case KEY_X:
    case KEY_DOWN:
        key = KEY_DOWN;
        break;
    default:
        return;
    }

    const int step = LvSettingUpdatePage3::metric(
        LvSettingUpdatePage3::LayoutMetric::ScrollStep);
    if (page->state_->dialog_open) {
        if (!page->state_->progress_bar && page->state_->dialog_content)
            scroll_dialog_by(page, key == KEY_UP ? step : -step);
    } else {
        scroll_body_by(page, key == KEY_UP ? step : -step);
    }
    lv_event_stop_processing(event);
}

void update_key_event(LvSettingUpdatePage3 *page, lv_event_t *event)
{
    if (!page || !event || lv_event_get_code(event) != LV_EVENT_KEY) return;
    const uint32_t key = lv_event_get_key(event);
    auto *state = page->state_.get();
    if (!state) return;

    if (state->dialog_open) {
        if (state->progress_bar || state->apps_updating || state->launcher_update) {
            if (key == LV_KEY_ESC) cancel_update(page, false);
        } else {
            handle_dialog_key(page, key);
        }
        lv_event_stop_processing(event);
        return;
    }

    if (state->request_pending || state->update_pending) {
        if (key == LV_KEY_ESC) cancel_update(page, false);
        lv_event_stop_processing(event);
        return;
    }

    if (key == LV_KEY_ESC || key == LV_KEY_LEFT) {
        cancel_update(page, true);
    } else if (key == LV_KEY_UP) {
        scroll_body_by(page, LvSettingUpdatePage3::metric(
                                   LvSettingUpdatePage3::LayoutMetric::ScrollStep));
    } else if (key == LV_KEY_DOWN) {
        scroll_body_by(page, -LvSettingUpdatePage3::metric(
                                    LvSettingUpdatePage3::LayoutMetric::ScrollStep));
    } else if (key == LV_KEY_ENTER || key == LV_KEY_RIGHT) {
        start_software_check(page);
    }
    lv_event_stop_processing(event);
}

} // namespace

LvSettingUpdatePage3::LvSettingUpdatePage3(lv_obj_t *parent,
                                           const NodeIter &page_node,
                                           std::function<void()> back_callback,
                                           settings_system::UpdateAction action)
    : state_(std::make_unique<State>())
{
    state_->action = action;
    if (page_node.node &&
        (page_node->label == "System update" || page_node->label == "Check system"))
        state_->action = settings_system::UpdateAction::CheckSystem;
    LeaveSelfPage = std::move(back_callback);
    create_ui(parent);
}

LvSettingUpdatePage3::~LvSettingUpdatePage3()
{
    if (state_) {
        state_->leaving = true;
        const std::string job_id = std::move(state_->job_id);
        state_->job_id.clear();
        stop_update_poll(state_.get());
        ++state_->generation;
        state_->request_pending = false;
        state_->update_pending = false;
        if (!job_id.empty()) schedule_job_cancel(job_id);
        if (state_->keyboard_root && state_->keyboard_event_dsc) {
            lv_obj_remove_event_dsc(state_->keyboard_root, state_->keyboard_event_dsc);
            state_->keyboard_event_dsc = nullptr;
        }
    }
    cancel_async_tasks();
    if (ComponensObj) {
        lv_obj_delete(ComponensObj);
        ComponensObj = nullptr;
    }
    state_.reset();
}

void LvSettingUpdatePage3::AnimateNextIn(std::function<void()> animate_over_func)
{
    if (animate_over_func) animate_over_func();
}

void LvSettingUpdatePage3::AnimateNextOut(std::function<void()> animate_over_func)
{
    if (animate_over_func) animate_over_func();
}

void LvSettingUpdatePage3::LoadNextPage()
{
}

void LvSettingUpdatePage3::LeaveNextPage()
{
    cancel_update(this, true);
}

void LvSettingUpdatePage3::create_ui(lv_obj_t *parent)
{
    if (!parent || !state_) return;
    using LayoutMetric = LvSettingUpdatePage3::LayoutMetric;
    ComponensObj = lv_obj_create(parent);
    if (!ComponensObj) return;
    configure_page(ComponensObj,
                   LvSettingUpdatePage3::metric(LayoutMetric::ScreenW),
                   LvSettingUpdatePage3::metric(LayoutMetric::ScreenH));
    DComponens::lvgl_bind_event(
        ComponensObj,
        LV_EVENT_KEY,
        nullptr,
        std::bind(&update_key_event, this, std::placeholders::_1));

    state_->title = create_label(ComponensObj,
                                 LvSettingUpdatePage3::metric(LayoutMetric::TextX),
                                 LvSettingUpdatePage3::metric(LayoutMetric::TitleY),
                                 LvSettingUpdatePage3::metric(LayoutMetric::TextW),
                                 "Software", 0x58A6FF, title_font());
    state_->summary = create_label(ComponensObj,
                                   LvSettingUpdatePage3::metric(LayoutMetric::TextX),
                                   LvSettingUpdatePage3::metric(LayoutMetric::SummaryY),
                                   LvSettingUpdatePage3::metric(LayoutMetric::TextW),
                                   "", 0xECECEC, settings_fonts::mono(12));
    if (state_->summary)
        lv_label_set_long_mode(state_->summary, LV_LABEL_LONG_SCROLL_CIRCULAR);

    state_->scroll_body = lv_obj_create(ComponensObj);
    if (state_->scroll_body) {
        lv_obj_set_pos(state_->scroll_body,
                       LvSettingUpdatePage3::metric(LayoutMetric::TextX),
                       LvSettingUpdatePage3::metric(LayoutMetric::ScrollY));
        lv_obj_set_size(state_->scroll_body,
                        LvSettingUpdatePage3::metric(LayoutMetric::TextW),
                        LvSettingUpdatePage3::metric(LayoutMetric::ScrollH));
        lv_obj_set_style_bg_opa(state_->scroll_body, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(state_->scroll_body, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(state_->scroll_body, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_row(state_->scroll_body, 3, LV_PART_MAIN);
        lv_obj_add_flag(state_->scroll_body, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(state_->scroll_body, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(state_->scroll_body, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(state_->scroll_body, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_flow(state_->scroll_body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(state_->scroll_body, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_add_event_cb(state_->scroll_body, &software_scroll_event_cb,
                            LV_EVENT_SCROLL_BEGIN, this);
        lv_obj_add_event_cb(state_->scroll_body, &software_scroll_event_cb,
                            LV_EVENT_SCROLL_END, this);

        const auto add_text = [this](const std::string &text, const lv_font_t *font,
                                     uint32_t color, bool wrap) -> lv_obj_t * {
            lv_obj_t *label = lv_label_create(state_->scroll_body);
            if (!label) return nullptr;
            lv_obj_set_width(label, LvSettingUpdatePage3::metric(LayoutMetric::TextW));
            lv_label_set_long_mode(label, wrap ? LV_LABEL_LONG_WRAP
                                               : LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_label_set_text(label, text.c_str());
            lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
            lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
            return label;
        };

        const std::string build_line =
            "Build " + std::string(launcher_build_date()) + " " +
            launcher_channel() + " (" + launcher_commit() + ")";
        add_text(build_line, settings_fonts::mono(12), 0x9A9A9A, false);
        add_text("Device: M5CardputerZero   LVGL: " + lvgl_version(),
                 settings_fonts::mono(12), 0x9A9A9A, false);

        lv_obj_t *separator = lv_obj_create(state_->scroll_body);
        if (separator) {
            lv_obj_set_width(separator, lv_pct(100));
            lv_obj_set_height(separator, 1);
            lv_obj_set_style_bg_color(separator, lv_color_hex(0x383838), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(separator, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(separator, 0, LV_PART_MAIN);
            lv_obj_remove_flag(separator, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(separator, LV_OBJ_FLAG_SCROLLABLE);
        }

        add_text("Installed software", settings_fonts::sans(12, LV_FREETYPE_FONT_STYLE_BOLD),
                 0x58A6FF, false);

        const std::vector<settings_software::InstalledApp> apps =
            settings_software::read_installed_apps();
        for (const settings_software::InstalledApp &app : apps) {
            State::AppRow row;
            row.package = app.package;
            row.short_name = app.short_name;
            row.version = app.version;
            row.name = lv_label_create(state_->scroll_body);
            if (row.name) {
                lv_obj_set_width(row.name, LvSettingUpdatePage3::metric(LayoutMetric::TextW));
                lv_label_set_long_mode(row.name, LV_LABEL_LONG_SCROLL_CIRCULAR);
                lv_label_set_text(row.name, (app.short_name + " " + app.version).c_str());
                lv_obj_set_style_text_font(
                    row.name, settings_fonts::sans(13, LV_FREETYPE_FONT_STYLE_BOLD),
                    LV_PART_MAIN);
                lv_obj_set_style_text_color(row.name, lv_color_hex(0xF2F2F2), LV_PART_MAIN);
                lv_obj_set_style_pad_top(row.name, 3, LV_PART_MAIN);
            }
            row.description = lv_label_create(state_->scroll_body);
            if (row.description) {
                lv_obj_set_width(row.description,
                                 LvSettingUpdatePage3::metric(LayoutMetric::TextW));
                lv_label_set_long_mode(row.description, LV_LABEL_LONG_WRAP);
                lv_label_set_text(row.description, app.description.c_str());
                lv_obj_set_style_text_font(row.description, settings_fonts::sans(12),
                                           LV_PART_MAIN);
                lv_obj_set_style_text_color(row.description, lv_color_hex(0xAAAAAA),
                                            LV_PART_MAIN);
            }
            state_->app_rows.push_back(std::move(row));
        }
        lv_obj_update_layout(state_->scroll_body);
    }

    state_->status_label = create_label(ComponensObj,
                                        LvSettingUpdatePage3::metric(LayoutMetric::TextX),
                                        LvSettingUpdatePage3::metric(LayoutMetric::StatusY),
                                        LvSettingUpdatePage3::metric(LayoutMetric::TextW),
                                        "", 0xF0C850, body_font());
    if (state_->status_label)
        lv_label_set_long_mode(state_->status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    state_->hint = create_label(ComponensObj,
                                LvSettingUpdatePage3::metric(LayoutMetric::TextX),
                                LvSettingUpdatePage3::metric(LayoutMetric::HintY),
                                LvSettingUpdatePage3::metric(LayoutMetric::TextW),
                                "", 0x46DC87, hint_font());

    state_->keyboard_root = lv_screen_active();
    if (state_->keyboard_root && LV_EVENT_KEYBOARD != 0) {
        state_->keyboard_event_dsc = lv_obj_add_event_cb(
            state_->keyboard_root, &software_keyboard_event_cb,
            static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), this);
    }

    render_update_page(this);
}

std::unique_ptr<DComponens::LvglComponensBase> settings_system_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback)
{
    if (is_update_page_label(page_node))
        return settings_update_page_factory(parent, page_node, std::move(back_callback));
    return std::make_unique<LvSettingSystemInfoPage3>(
        parent, page_node, std::move(back_callback), SettingsSystemPageKind::Auto);
}

std::unique_ptr<DComponens::LvglComponensBase> settings_ethernet_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback)
{
    return std::make_unique<LvSettingSystemInfoPage3>(
        parent, page_node, std::move(back_callback), SettingsSystemPageKind::Ethernet);
}

std::unique_ptr<DComponens::LvglComponensBase> settings_account_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback)
{
    return std::make_unique<LvSettingSystemInfoPage3>(
        parent, page_node, std::move(back_callback), SettingsSystemPageKind::Account);
}

std::unique_ptr<DComponens::LvglComponensBase> settings_update_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback)
{
    return std::make_unique<LvSettingUpdatePage3>(
        parent, page_node, std::move(back_callback), settings_system::UpdateAction::UpdateLauncher);
}

std::unique_ptr<DComponens::LvglComponensBase> settings_system_info_page3_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback)
{
    return settings_system_page_factory(parent, page_node, std::move(back_callback));
}

std::unique_ptr<DComponens::LvglComponensBase> settings_ethernet_page3_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback)
{
    return settings_ethernet_page_factory(parent, page_node, std::move(back_callback));
}

std::unique_ptr<DComponens::LvglComponensBase> settings_account_page3_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback)
{
    return settings_account_page_factory(parent, page_node, std::move(back_callback));
}

std::unique_ptr<DComponens::LvglComponensBase> settings_update_page3_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback)
{
    return settings_update_page_factory(parent, page_node, std::move(back_callback));
}
