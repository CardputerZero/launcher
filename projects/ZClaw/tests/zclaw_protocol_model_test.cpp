#include "zclaw_chat_event.h"
#include "zclaw_chat_layout.h"
#include "zclaw_chat_stream_model.h"
#include "zclaw_file_downloader.h"
#include "zclaw_gateway_response_model.h"
#include "zclaw_http_cancellation.h"
#include "zclaw_http_client_policy.h"
#include "zclaw_protocol.h"
#include "zclaw_quickstart_model.h"
#include "zclaw_setup_progress_model.h"

#include <cassert>
#include <memory>
#include <string>

int main()
{
    const zclaw::HttpClientTimeouts probe_timeouts =
        zclaw::http_client_timeouts(zclaw::HttpClientProfile::Probe);
    assert(probe_timeouts.connection_seconds == 2);
    assert(probe_timeouts.read_seconds == 2);
    assert(probe_timeouts.write_seconds == 2);
    const zclaw::HttpClientTimeouts download_timeouts =
        zclaw::http_client_timeouts(zclaw::HttpClientProfile::Download);
    assert(download_timeouts.connection_seconds == 30);
    assert(download_timeouts.read_seconds == 0);
    assert(download_timeouts.write_seconds == 30);

    std::string download_error;
    assert(!zclaw::FileDownloader().download(
        "not-a-url", "/tmp/zclaw-unused-download", &download_error));
    assert(download_error == "Invalid download URL.");
    download_error.clear();
    assert(!zclaw::FileDownloader().download(
        "http://example.com/file", "/zclaw-missing/download", &download_error));
    assert(download_error == "Cannot write /zclaw-missing/download");
    auto cancelled_downloads =
        std::make_shared<zclaw::HttpCancellationRegistry>();
    cancelled_downloads->shutdown();
    download_error.clear();
    assert(!zclaw::FileDownloader(cancelled_downloads).download(
        "http://example.com/file", "/zclaw-missing/download", &download_error));
    assert(download_error == "Download cancelled.");

    zclaw::protocol::HttpUrl parsed_url;
    assert(zclaw::protocol::split_http_url("https://example.com/path?q=1", &parsed_url));
    assert(parsed_url.base == "https://example.com");
    assert(parsed_url.path == "/path?q=1");
    assert(!zclaw::protocol::split_http_url("not-a-url", &parsed_url));

    zclaw::UiConfig config;
    config.webhook_url = "https://example.com/webhook";
    config.agent_alias = "agent name";
    config.bearer_token = "token/value";
    assert(zclaw::protocol::gateway_http_base(config) == "https://example.com");
    assert(zclaw::protocol::gateway_ws_url(config) ==
           "wss://example.com/ws/chat?agent=agent%20name&token=token%2Fvalue&session_id=zclaw-ui&name=ZClaw");
    assert(zclaw::protocol::webhook_endpoint(config) ==
           "https://example.com/webhook?agent=agent%20name");

    const std::string webhook_json = zclaw::protocol::make_webhook_message("line\n\"quoted\"");
    assert(webhook_json == "{\"message\":\"line\\n\\\"quoted\\\"\"}");
    const std::string approval_json =
        zclaw::protocol::make_ws_approval_response("request-1", "");
    assert(approval_json ==
           "{\"decision\":\"deny\",\"request_id\":\"request-1\","
           "\"type\":\"approval_response\"}");

    zclaw::OperationResult gateway_result = zclaw::interpret_pairing_response(
        config, 200, "{\"token\":\"paired-token\"}");
    assert(gateway_result.ok);
    assert(gateway_result.config.bearer_token == "paired-token");
    gateway_result = zclaw::interpret_pairing_response(
        config, 401, "denied");
    assert(!gateway_result.ok);
    assert(gateway_result.text ==
           "Pairing request failed.\nHTTP 401\ndenied");
    gateway_result = zclaw::interpret_pairing_response(
        config, 200, "{\"error\":\"bad code\"}");
    assert(!gateway_result.ok);
    assert(gateway_result.text == "Pairing failed: bad code");
    gateway_result = zclaw::interpret_pairing_response(config, 200, "invalid");
    assert(!gateway_result.ok && gateway_result.text == "Pairing failed.\ninvalid");
    gateway_result = zclaw::interpret_pairing_response(
        config, 200, "{\"token\":42}");
    assert(!gateway_result.ok);
    gateway_result = zclaw::interpret_pairing_response(config, 200, "{}");
    assert(!gateway_result.ok);

    gateway_result = zclaw::interpret_webhook_response(
        config, 200, "{\"response\":\"hello\"}");
    assert(gateway_result.ok && gateway_result.text == "hello");
    gateway_result = zclaw::interpret_webhook_response(
        config, 200, "{\"response\":\"ignored\",\"error\":\"failed\"}");
    assert(!gateway_result.ok);
    assert(gateway_result.text == "ZeroClaw error: failed");
    gateway_result = zclaw::interpret_webhook_response(config, 503, "offline");
    assert(!gateway_result.ok);
    assert(gateway_result.text ==
           "Webhook request failed.\nHTTP 503\noffline");
    gateway_result = zclaw::interpret_webhook_response(config, 200, "plain reply");
    assert(gateway_result.ok && gateway_result.text == "plain reply");
    gateway_result = zclaw::interpret_webhook_response(
        config, 200, "{\"response\":42}");
    assert(gateway_result.ok && gateway_result.text == "{\"response\":42}");
    gateway_result = zclaw::interpret_webhook_response(config, 200, "{}");
    assert(gateway_result.ok && gateway_result.text == "{}");

    zclaw::ChatEvent event = zclaw::parse_chat_event(
        "{\"type\":\"chunk\",\"content\":\"partial\"}");
    assert(event.type == zclaw::ChatEventType::Chunk);
    assert(event.content == "partial");
    event = zclaw::parse_chat_event(
        "{\"type\":\"approval_request\",\"request_id\":\"req-1\","
        "\"tool\":\"shell\",\"arguments_summary\":\"ls\",\"timeout_secs\":45}");
    assert(event.type == zclaw::ChatEventType::ApprovalRequest);
    assert(event.approval.request_id == "req-1");
    assert(event.approval.tool == "shell");
    assert(event.approval.summary == "ls");
    assert(event.approval.timeout_secs == 45);
    event = zclaw::parse_chat_event(
        "{\"type\":\"approval_request\",\"request_id\":\"req-2\"}");
    assert(event.approval.timeout_secs == 120);
    event = zclaw::parse_chat_event(
        "{\"type\":\"approval_request\",\"request_id\":\"req-3\","
        "\"timeout_secs\":-1}");
    assert(event.type == zclaw::ChatEventType::ApprovalRequest);
    assert(event.approval.timeout_secs == 120);
    event = zclaw::parse_chat_event(
        "{\"type\":\"approval_request\",\"request_id\":\"req-4\","
        "\"timeout_secs\":3601}");
    assert(event.approval.timeout_secs == 120);
    event = zclaw::parse_chat_event(
        "{\"type\":\"approval_request\",\"request_id\":\"req-4b\","
        "\"timeout_secs\":9223372036854775808}");
    assert(event.approval.timeout_secs == 120);
    event = zclaw::parse_chat_event(
        "{\"type\":\"approval_request\",\"request_id\":\"req-5\","
        "\"timeout_secs\":\"45\"}");
    assert(event.approval.timeout_secs == 120);
    assert(zclaw::parse_chat_event(
               "{\"type\":\"approval_request\",\"tool\":\"shell\"}")
               .type == zclaw::ChatEventType::Unknown);
    assert(zclaw::parse_chat_event(
               "{\"type\":\"approval_request\",\"request_id\":\"req\","
               "\"tool\":42}")
               .type == zclaw::ChatEventType::Unknown);
    event = zclaw::parse_chat_event(
        "{\"type\":\"done\",\"full_response\":\"complete\"}");
    assert(event.type == zclaw::ChatEventType::Done);
    assert(event.content == "complete");
    event = zclaw::parse_chat_event("{\"type\":\"error\",\"message\":\"failed\"}");
    assert(event.type == zclaw::ChatEventType::Error);
    assert(event.content == "failed");
    assert(zclaw::parse_chat_event("{\"type\":\"other\"}").type ==
           zclaw::ChatEventType::Unknown);
    assert(zclaw::parse_chat_event("invalid").type == zclaw::ChatEventType::Unknown);
    assert(zclaw::parse_chat_event("[]").type == zclaw::ChatEventType::Unknown);
    assert(zclaw::parse_chat_event("{}").type == zclaw::ChatEventType::Unknown);
    assert(zclaw::parse_chat_event("{\"type\":42}").type ==
           zclaw::ChatEventType::Unknown);
    assert(zclaw::parse_chat_event("{\"type\":\"chunk\"}").type ==
           zclaw::ChatEventType::Unknown);
    assert(zclaw::parse_chat_event(
               "{\"type\":\"done\",\"full_response\":42}")
               .type == zclaw::ChatEventType::Unknown);

    zclaw::ChatStreamState stream;
    zclaw::ChatStreamUpdate stream_update = stream.accept(
        {zclaw::ChatEventType::Chunk, "partial ", {}});
    assert(!stream_update.finished);
    stream.accept({zclaw::ChatEventType::Chunk, "reply", {}});
    zclaw::ApprovalRequest stream_approval;
    stream_approval.request_id = "approve-1";
    stream_update = stream.accept(
        {zclaw::ChatEventType::ApprovalRequest, "", stream_approval});
    assert(stream_update.approval_requested);
    assert(stream_update.approval.request_id == "approve-1");
    stream_update = stream.accept({zclaw::ChatEventType::Done, "", {}});
    assert(stream_update.finished);
    assert(stream.finished());
    assert(stream.succeeded());
    assert(stream.response() == "partial reply");

    zclaw::ChatStreamState failed_stream;
    stream_update = failed_stream.accept(
        {zclaw::ChatEventType::Error, "permission denied", {}});
    assert(stream_update.finished);
    assert(!failed_stream.succeeded());
    assert(failed_stream.response() == "ZeroClaw error: permission denied");
    failed_stream.accept({zclaw::ChatEventType::Done, "must be ignored", {}});
    assert(!failed_stream.succeeded());
    assert(failed_stream.response() == "ZeroClaw error: permission denied");

    zclaw::ProviderConfig incomplete_provider;
    const zclaw::ProviderConfig normalized =
        zclaw::normalize_setup_provider(incomplete_provider);
    assert(normalized.alias == "zclaw");
    assert(normalized.family == "openai");
    assert(normalized.model == "gpt-4.1-mini");
    const zclaw::ProviderConfig incomplete_ollama = {
        "local", "ollama", "llama", "", ""
    };
    assert(zclaw::normalize_setup_provider(incomplete_ollama).uri ==
           "http://127.0.0.1:11434");
    assert(zclaw::extract_pairing_code("Pairing code: 123456\n") == "123456");
    assert(zclaw::extract_pairing_code("12 3456").empty());
    assert(zclaw::extract_pairing_code("old 1234567") == "123456");

    const zclaw::ChatBubbleLayout short_bubble = zclaw::user_bubble_layout(2, 5);
    assert(short_bubble.width == 38 && short_bubble.height == 27);
    const zclaw::ChatBubbleLayout wide_bubble = zclaw::user_bubble_layout(500, 30);
    assert(wide_bubble.width == 198 && wide_bubble.height == 42);
    assert(zclaw::assistant_bubble_height(5) == 41);
    assert(zclaw::assistant_bubble_height(50) == 62);
    assert(zclaw::chat_scroll_delta(0, 100, 24) == 0);
    assert(zclaw::chat_scroll_delta(10, 100, 24) == 10);
    assert(zclaw::chat_scroll_delta(50, 100, 24) == 24);
    assert(zclaw::chat_scroll_delta(100, 0, -24) == 0);
    assert(zclaw::chat_scroll_delta(100, 10, -24) == -10);
    assert(zclaw::chat_scroll_delta(100, 50, -24) == -24);
    assert(zclaw::chat_scroll_delta(-1, -1, 0) == 0);

    zclaw::SetupProgress progress;
    progress.status = "Preparing";
    progress.percent = -5;
    zclaw::SetupProgressPresentation presentation =
        zclaw::present_setup_progress(progress);
    assert(presentation.status == "Preparing");
    assert(presentation.detail == "Preparing");
    assert(presentation.percent == 0);
    assert(presentation.percent_text == "0%");
    assert(presentation.speed.empty());

    progress.status = "Downloading";
    progress.percent = 125;
    progress.downloaded_bytes = 512 * 1024;
    progress.total_bytes = 2 * 1024 * 1024;
    progress.bytes_per_second = 256 * 1024;
    progress.source_url = "https://example.test/releases/zeroclaw.tar.gz";
    progress.destination_path = "/tmp/install dir/zeroclaw.tar.gz";
    presentation = zclaw::present_setup_progress(progress);
    assert(presentation.detail == "0.5 / 2.0 MB");
    assert(presentation.percent == 100);
    assert(presentation.percent_text == "100%");
    assert(presentation.speed == "256 KB/s");
    assert(presentation.source_url ==
           "https://example.test/releases/zeroclaw.tar.gz");
    assert(presentation.destination_path ==
           "/tmp/install dir/zeroclaw.tar.gz");
    progress.bytes_per_second = 1.5 * 1024 * 1024;
    assert(zclaw::present_setup_progress(progress).speed == "1.5 MB/s");
    progress.bytes_per_second = 0.0;
    assert(zclaw::present_setup_progress(progress).speed == "Waiting for data");
    progress.total_bytes = 0;
    progress.bytes_per_second = 512.0;
    assert(zclaw::present_setup_progress(progress).speed == "512 B/s");
    progress.bytes_per_second = 1024.0;
    assert(zclaw::present_setup_progress(progress).speed == "1 KB/s");

    const zclaw::ChatScrollbarLayout idle_scrollbar =
        zclaw::chat_scrollbar_layout(0, 0);
    assert(idle_scrollbar.y == 20 && idle_scrollbar.height == 126);
    const zclaw::ChatScrollbarLayout middle_scrollbar =
        zclaw::chat_scrollbar_layout(100, 100);
    assert(middle_scrollbar.y > 20 && middle_scrollbar.y < 128);
    assert(middle_scrollbar.height >= 18 && middle_scrollbar.height < 126);
    return 0;
}
