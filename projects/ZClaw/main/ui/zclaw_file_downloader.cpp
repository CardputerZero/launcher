#include "zclaw_file_downloader.h"

#include "zclaw_http_cancellation.h"
#include "zclaw_http_client_policy.h"
#include "zclaw_lhv_http_client.h"
#include "zclaw_protocol.h"

#include <chrono>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <memory>
#include <utility>
#include <unistd.h>
#include <strings.h>

namespace zclaw {
namespace {

constexpr int kMaxRedirects = 5;
constexpr int kIdleTimeoutSeconds = 30;
constexpr int kRequestSafetyTimeoutSeconds = 12 * 60 * 60;

bool is_redirect(int status_code)
{
    return status_code == 301 || status_code == 302 || status_code == 303 ||
           status_code == 307 || status_code == 308;
}

bool parse_content_range(const std::string &value, long *from, long *to,
                         long *total)
{
    return from && to && total &&
           std::sscanf(value.c_str(), "bytes %ld-%ld/%ld", from, to, total) == 3;
}

bool parse_unsatisfied_range(const std::string &value, long *total)
{
    return total && std::sscanf(value.c_str(), "bytes */%ld", total) == 1;
}

std::string redirect_url(const std::string &current_url,
                         const std::string &location)
{
    protocol::HttpUrl absolute;
    if (protocol::split_http_url(location, &absolute))
        return location;

    protocol::HttpUrl current;
    if (!protocol::split_http_url(current_url, &current) || location.empty())
        return {};
    if (location.front() == '/')
        return current.base + location;

    std::string directory = current.path;
    const std::size_t query = directory.find_first_of("?#");
    if (query != std::string::npos)
        directory.resize(query);
    const std::size_t slash = directory.rfind('/');
    directory = slash == std::string::npos ? "/" : directory.substr(0, slash + 1);
    return current.base + directory + location;
}

std::string header_value(const HttpMessage &message, const char *name)
{
    for (const auto &header : message.headers) {
        if (::strcasecmp(header.first.c_str(), name) == 0)
            return header.second;
    }
    return {};
}

std::int64_t steady_milliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

FileDownloader::FileDownloader()
    : client_(std::make_shared<LhvHttpClient>())
{
}

FileDownloader::FileDownloader(
    std::shared_ptr<HttpCancellationRegistry> cancellation)
    : cancellation_(std::move(cancellation)),
      client_(std::make_shared<LhvHttpClient>())
{
}

FileDownloader::FileDownloader(
    std::shared_ptr<HttpCancellationRegistry> cancellation,
    std::shared_ptr<LhvHttpClient> client)
    : cancellation_(std::move(cancellation)), client_(std::move(client))
{
}

bool FileDownloader::download(const std::string &url_text,
                              const std::string &output_path,
                              std::string *error,
                              const ProgressHandler &progress_handler) const
{
    protocol::HttpUrl url;
    if (!protocol::split_http_url(url_text, &url)) {
        if (error)
            *error = "Invalid download URL.";
        return false;
    }

    if (!client_) {
        if (error)
            *error = "Download HTTP client unavailable.";
        return false;
    }
    if (cancellation_ && cancellation_->shutdown_requested()) {
        if (error)
            *error = "Download cancelled.";
        return false;
    }

    std::error_code filesystem_error;
    const std::filesystem::file_status output_status =
        std::filesystem::symlink_status(output_path, filesystem_error);
    if (filesystem_error &&
        filesystem_error != std::errc::no_such_file_or_directory) {
        if (error)
            *error = "Cannot write " + output_path;
        return false;
    }
    std::size_t resume_offset = 0;
    if (output_status.type() == std::filesystem::file_type::regular) {
        const std::uintmax_t size =
            std::filesystem::file_size(output_path, filesystem_error);
        if (filesystem_error ||
            size > static_cast<std::uintmax_t>(
                       std::numeric_limits<std::size_t>::max())) {
            if (error)
                *error = "Cannot inspect " + output_path;
            return false;
        }
        resume_offset = static_cast<std::size_t>(size);
    } else if (output_status.type() != std::filesystem::file_type::not_found) {
        if (error)
            *error = "Cannot write " + output_path;
        return false;
    }

    std::ofstream output;
    auto last_time = std::chrono::steady_clock::now();
    std::size_t last_bytes = resume_offset;
    std::size_t received_bytes = resume_offset;
    std::size_t total_bytes = 0;
    bool output_open_failed = false;
    bool range_mismatch = false;
    bool range_complete = false;
    std::atomic<std::int64_t> last_activity_ms{steady_milliseconds()};
    auto stream_callback = [&](HttpMessage *response, http_parser_state state,
                               const char *data, std::size_t length) {
        last_activity_ms.store(steady_milliseconds(), std::memory_order_relaxed);
        HttpResponse *http_response = static_cast<HttpResponse *>(response);
        if (is_redirect(http_response->status_code))
            return;
        if (state == HP_HEADERS_COMPLETE) {
            const int status_code = http_response->status_code;
            if (status_code == 416 && resume_offset > 0) {
                long complete_size = 0;
                range_complete = parse_unsatisfied_range(
                                     header_value(*response, "Content-Range"),
                                     &complete_size) &&
                                 complete_size >= 0 &&
                                 static_cast<std::size_t>(complete_size) ==
                                     resume_offset;
                return;
            }
            if (status_code != 200 && status_code != 206)
                return;
            bool append = status_code == 206 && resume_offset > 0;
            long range_from = 0;
            long range_to = 0;
            long range_total = 0;
            if (status_code == 206 && parse_content_range(
                                          header_value(*response, "Content-Range"),
                                          &range_from, &range_to, &range_total)) {
                if (range_from < 0 ||
                    static_cast<std::size_t>(range_from) != resume_offset) {
                    range_mismatch = true;
                    return;
                }
                if (range_total > 0)
                    total_bytes = static_cast<std::size_t>(range_total);
            } else if (status_code == 206 && resume_offset > 0) {
                range_mismatch = true;
                return;
            }
            if (!append) {
                resume_offset = 0;
                received_bytes = 0;
                last_bytes = 0;
            }
            output.open(output_path, std::ios::binary |
                                         (append ? std::ios::app : std::ios::trunc));
            if (!output) {
                output_open_failed = true;
                return;
            }
            const std::string content_length =
                header_value(*response, "Content-Length");
            if (!content_length.empty()) {
                try {
                    const std::size_t body_bytes = static_cast<std::size_t>(
                        std::stoull(content_length));
                    if (total_bytes == 0)
                        total_bytes = received_bytes + body_bytes;
                } catch (...) {
                    if (total_bytes == 0)
                        total_bytes = 0;
                }
            }
            last_time = std::chrono::steady_clock::now();
            if (progress_handler && received_bytes > 0)
                progress_handler({received_bytes, total_bytes, 0.0});
        } else if (state == HP_BODY && data && length) {
            if (!output || range_mismatch || output_open_failed)
                return;
            output.write(data, static_cast<std::streamsize>(length));
            received_bytes += length;
            const auto now = std::chrono::steady_clock::now();
            const double seconds =
                std::chrono::duration<double>(now - last_time).count();
            if (seconds >= 0.2 || (total_bytes && received_bytes == total_bytes)) {
                const double speed = seconds > 0.0
                                         ? static_cast<double>(received_bytes - last_bytes) /
                                               seconds
                                         : 0.0;
                if (progress_handler)
                    progress_handler({received_bytes, total_bytes, speed});
                last_time = now;
                last_bytes = received_bytes;
            }
        } else if (state == HP_MESSAGE_COMPLETE &&
                   received_bytes > last_bytes) {
            const auto now = std::chrono::steady_clock::now();
            const double seconds =
                std::chrono::duration<double>(now - last_time).count();
            const double speed = seconds > 0.0
                                     ? static_cast<double>(received_bytes - last_bytes) /
                                           seconds
                                     : 0.0;
            if (progress_handler)
                progress_handler({received_bytes, total_bytes, speed});
            last_time = now;
            last_bytes = received_bytes;
        }
    };

    std::string request_url = url.base + url.path;
    HttpResponsePtr response;
    bool idle_timeout = false;
    bool cancelled = false;
    for (int redirect_count = 0; redirect_count <= kMaxRedirects;
         ++redirect_count) {
        auto request = std::make_shared<HttpRequest>();
        request->method = HTTP_GET;
        request->url = request_url;
        configure_http_request(*request, HttpClientProfile::Download);
        request->timeout = kRequestSafetyTimeoutSeconds;
        request->redirect = false;
        if (resume_offset > 0) {
            if (resume_offset > static_cast<std::size_t>(
                                    std::numeric_limits<long>::max())) {
                if (error)
                    *error = "Download cache is too large to resume.";
                return false;
            }
            request->SetHeader(
                "Range", "bytes=" + std::to_string(resume_offset) + "-");
        }
        request->http_cb = stream_callback;

        auto response_promise = std::make_shared<std::promise<HttpResponsePtr>>();
        std::future<HttpResponsePtr> response_future = response_promise->get_future();
        if (!client_->send(std::move(request), [response_promise](
                                                   const HttpResponsePtr &value) {
                response_promise->set_value(value);
            })) {
            if (error)
                *error = "Download failed.\nRequest was not submitted.";
            return false;
        }
        last_activity_ms.store(steady_milliseconds(), std::memory_order_relaxed);
        while (response_future.wait_for(std::chrono::milliseconds(250)) !=
               std::future_status::ready) {
            cancelled =
                cancellation_ && cancellation_->shutdown_requested();
            const bool inactive =
                steady_milliseconds() -
                    last_activity_ms.load(std::memory_order_relaxed) >=
                kIdleTimeoutSeconds * 1000LL;
            if (!cancelled && !inactive)
                continue;
            idle_timeout = inactive;
            client_->shutdown();
            break;
        }
        response = response_future.get();
        if (!response || !is_redirect(response->status_code))
            break;
        if (redirect_count == kMaxRedirects) {
            if (error)
                *error = "Download failed.\nToo many redirects.";
            return false;
        }
        request_url = redirect_url(request_url,
                                   header_value(*response, "Location"));
        if (request_url.empty())
            break;
    }
    if (output.is_open())
        output.close();

    if (!response) {
        if (error)
            *error = cancelled
                         ? "Download cancelled."
                         : idle_timeout
                         ? "Download failed.\nNo data received for 30 seconds."
                         : "Download failed.\nTCP connection closed.";
        return false;
    }
    if (range_mismatch) {
        if (resume_offset == 0) {
            if (error)
                *error = "Download failed.\nServer returned an invalid byte range.";
            return false;
        }
        std::ofstream reset(output_path, std::ios::binary | std::ios::trunc);
        if (!reset) {
            if (error)
                *error = "Download failed.\nCould not reset invalid cache.";
            return false;
        }
        reset.close();
        return download(url_text, output_path, error, progress_handler);
    }
    if (response->status_code == 416 && resume_offset > 0) {
        if (range_complete)
            return true;
        std::ofstream reset(output_path, std::ios::binary | std::ios::trunc);
        if (!reset) {
            if (error)
                *error = "Download failed.\nCould not reset invalid cache.";
            return false;
        }
        reset.close();
        return download(url_text, output_path, error, progress_handler);
    }
    if (response->status_code < 200 || response->status_code >= 300) {
        if (error)
            *error = "Download failed.\nHTTP " +
                     std::to_string(response->status_code);
        return false;
    }
    if (output_open_failed || !output) {
        if (error)
            *error = "Download failed.\nCould not write output file.";
        return false;
    }
    if (total_bytes > 0 && received_bytes != total_bytes) {
        if (error)
            *error = "Download failed.\nConnection closed before completion.";
        return false;
    }
    return true;
}

}  // namespace zclaw
