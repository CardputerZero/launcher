/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_http_worker.hpp"

#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef APPLAUNCH_USE_LIBCURL
#include <curl/curl.h>
#endif

namespace settings_http {
namespace {

constexpr std::size_t kMaxBodyBytes = 512 * 1024;

#ifdef APPLAUNCH_USE_LIBCURL
struct Transfer {
    CURL *easy = nullptr;
    std::size_t index = 0;
    std::vector<char> body;
    bool overflow = false;
    const CancelFlag *cancel = nullptr;
    curl_slist *headers = nullptr;
};

struct Job {
    std::vector<Request> requests;
    const CancelFlag *cancel = nullptr;
    std::vector<Response> responses;
    int max_concurrency = 6;
    bool done = false;
    std::mutex mutex;
    std::condition_variable condition;
};

class Worker {
public:
    Worker()
    {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return;
        multi_ = curl_multi_init();
        if (!multi_) return;
        thread_ = std::thread([this]() { run(); });
    }

    ~Worker()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        if (thread_.joinable()) thread_.join();
        if (multi_) curl_multi_cleanup(multi_);
        curl_global_cleanup();
    }

    bool available() const { return multi_ != nullptr && thread_.joinable(); }

    void wake()
    {
#if defined(LIBCURL_VERSION_NUM) && LIBCURL_VERSION_NUM >= 0x074400
        if (multi_) curl_multi_wakeup(multi_);
#endif
    }

    std::vector<Response> submit(std::vector<Request> requests,
                                 const CancelFlag *cancel,
                                 int max_concurrency)
    {
        auto job = std::make_shared<Job>();
        job->requests = std::move(requests);
        job->cancel = cancel;
        job->responses.resize(job->requests.size());
        job->max_concurrency = std::max(1, std::min(8, max_concurrency));
        if (!available()) return {};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(job);
        }
        condition_.notify_one();
        std::unique_lock<std::mutex> lock(job->mutex);
        job->condition.wait(lock, [&job]() { return job->done; });
        return job->responses;
    }

private:
    void run()
    {
        for (;;) {
            std::shared_ptr<Job> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return stop_ || !queue_.empty(); });
                if (stop_) return;
                job = queue_.front();
                queue_.pop_front();
            }
            process(*job);
            {
                std::lock_guard<std::mutex> lock(job->mutex);
                job->done = true;
            }
            job->condition.notify_all();
        }
    }

    static std::size_t write_callback(char *data, std::size_t size,
                                      std::size_t count, void *userdata)
    {
        auto *transfer = static_cast<Transfer *>(userdata);
        if (!transfer) return 0;
        const std::size_t bytes = size * count;
        if (transfer->body.size() + bytes > kMaxBodyBytes) {
            transfer->overflow = true;
            return 0;
        }
        transfer->body.insert(transfer->body.end(), data, data + bytes);
        return bytes;
    }

    static int progress_callback(void *userdata, curl_off_t, curl_off_t, curl_off_t,
                                 curl_off_t)
    {
        auto *transfer = static_cast<Transfer *>(userdata);
        if (transfer && transfer->cancel &&
            transfer->cancel->load(std::memory_order_acquire))
            return 1;
        return 0;
    }

    Transfer *start_transfer(std::size_t index, const Request &request,
                             const CancelFlag *cancel)
    {
        auto *transfer = new Transfer;
        transfer->index = index;
        transfer->cancel = cancel;
        transfer->easy = curl_easy_init();
        if (!transfer->easy) {
            delete transfer;
            return nullptr;
        }
        CURL *easy = transfer->easy;
        curl_easy_setopt(easy, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS,
                         static_cast<long>(std::min(5000, request.timeout_ms)));
        curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS,
                         static_cast<long>(request.timeout_ms));
        curl_easy_setopt(easy, CURLOPT_USERAGENT, request.user_agent.c_str());
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, transfer);
        curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(easy, CURLOPT_XFERINFODATA, transfer);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
        if (!request.accept_header.empty()) {
            const std::string header = "Accept: " + request.accept_header;
            transfer->headers = curl_slist_append(transfer->headers, header.c_str());
            if (transfer->headers)
                curl_easy_setopt(easy, CURLOPT_HTTPHEADER, transfer->headers);
        }
        if (curl_multi_add_handle(multi_, easy) != CURLM_OK) {
            if (transfer->headers) curl_slist_free_all(transfer->headers);
            curl_easy_cleanup(easy);
            delete transfer;
            return nullptr;
        }
        return transfer;
    }

    void finish_transfer(Transfer *transfer, CURLcode code)
    {
        Response &response = (*current_responses_)[transfer->index];
        long status = 0;
        char *effective = nullptr;
        curl_easy_getinfo(transfer->easy, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_getinfo(transfer->easy, CURLINFO_EFFECTIVE_URL, &effective);
        response.status = status;
        response.effective_url = effective ? effective : "";
        response.body.assign(transfer->body.begin(), transfer->body.end());
        response.code = static_cast<int>(code);
        response.ok = code == CURLE_OK && status >= 200 && status < 300;
        if (code == CURLE_OK && status >= 400) response.code = 22;
        if (!response.ok) response.error = curl_easy_strerror(code);
        curl_multi_remove_handle(multi_, transfer->easy);
        curl_easy_cleanup(transfer->easy);
        if (transfer->headers) curl_slist_free_all(transfer->headers);
        delete transfer;
    }

    void process(Job &job)
    {
        current_responses_ = &job.responses;
        std::vector<Transfer *> active;
        std::size_t next = 0;
        const auto add_next = [&]() {
            while (next < job.requests.size() &&
                   static_cast<int>(active.size()) < job.max_concurrency) {
                Transfer *transfer =
                    start_transfer(next, job.requests[next], job.cancel);
                job.responses[next].code = transfer ? -1 : -ENOMEM;
                if (!transfer) {
                    ++next;
                    continue;
                }
                active.push_back(transfer);
                ++next;
            }
        };

        add_next();
        while (!active.empty()) {
            if (job.cancel && job.cancel->load(std::memory_order_acquire)) {
                for (Transfer *transfer : active) {
                    job.responses[transfer->index].code = -ECANCELED;
                    curl_multi_remove_handle(multi_, transfer->easy);
                    curl_easy_cleanup(transfer->easy);
                    if (transfer->headers) curl_slist_free_all(transfer->headers);
                    delete transfer;
                }
                for (std::size_t i = next; i < job.requests.size(); ++i)
                    job.responses[i].code = -ECANCELED;
                current_responses_ = nullptr;
                return;
            }

            int running = 0;
            curl_multi_perform(multi_, &running);
            CURLMsg *message = nullptr;
            int left = 0;
            while ((message = curl_multi_info_read(multi_, &left)) != nullptr) {
                if (message->msg != CURLMSG_DONE) continue;
                CURL *easy = message->easy_handle;
                auto it = std::find_if(active.begin(), active.end(),
                                       [easy](Transfer *transfer) {
                                           return transfer->easy == easy;
                                       });
                if (it == active.end()) continue;
                Transfer *transfer = *it;
                active.erase(it);
                finish_transfer(transfer, message->data.result);
            }
            add_next();
            if (!active.empty())
                curl_multi_poll(multi_, nullptr, 0, 50, nullptr);
        }
        for (std::size_t i = next; i < job.requests.size(); ++i) {
            if (job.responses[i].code == -1) job.responses[i].code = -ENOENT;
        }
        current_responses_ = nullptr;
    }

    CURLM *multi_ = nullptr;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::shared_ptr<Job>> queue_;
    bool stop_ = false;
    std::vector<Response> *current_responses_ = nullptr;
};

Worker &worker()
{
    static Worker instance;
    return instance;
}
#endif  // APPLAUNCH_USE_LIBCURL

Response capture_with_curl(const Request &request, const CancelFlag *cancel)
{
    Response response;
    if (cancel && cancel->load(std::memory_order_acquire)) {
        response.code = -ECANCELED;
        return response;
    }
    const int seconds = std::max(1, (request.timeout_ms + 999) / 1000);
    std::list<std::string> arguments = {
        "CaptureArgv", "timeout", std::to_string(seconds + 5), "curl", "-fsSL",
        "--max-time", std::to_string(seconds), "--connect-timeout", "5",
        "-A", request.user_agent};
    if (!request.accept_header.empty())
        arguments.insert(arguments.end(), {"-H", "Accept: " + request.accept_header});
    arguments.insert(arguments.end(),
                     {"-w", "\n%{url_effective}", request.url});

    std::string output;
    int code = -1;
    try {
        cp0_signal_process_api(
            std::move(arguments), [&](int result, std::string data) {
                code = result;
                output = std::move(data);
            });
    } catch (...) {
        response.code = -1;
        response.error = "process api unavailable";
        return response;
    }
    if (code != 0) {
        response.code = code;
        response.error = "curl subprocess failed";
        return response;
    }
    const std::size_t split = output.rfind('\n');
    if (split != std::string::npos) {
        response.body = output.substr(0, split);
        response.effective_url = output.substr(split + 1);
    } else {
        response.body = std::move(output);
    }
    response.status = 200;
    response.ok = true;
    response.code = 0;
    return response;
}

}  // namespace

bool libcurl_available()
{
#ifdef APPLAUNCH_USE_LIBCURL
    return worker().available();
#else
    return false;
#endif
}

void wake()
{
#ifdef APPLAUNCH_USE_LIBCURL
    if (worker().available()) worker().wake();
#endif
}

std::vector<Response> download_batch(const std::vector<Request> &requests,
                                     const CancelFlag *cancel,
                                     int max_concurrency)
{
    (void)max_concurrency;
    if (requests.empty()) return {};
#ifdef APPLAUNCH_USE_LIBCURL
    if (worker().available()) {
        std::vector<Response> responses =
            worker().submit(requests, cancel, max_concurrency);
        if (responses.size() == requests.size()) return responses;
    }
#endif
    std::vector<Response> responses;
    responses.reserve(requests.size());
    for (const Request &request : requests) {
        if (cancel && cancel->load(std::memory_order_acquire)) {
            Response response;
            response.code = -ECANCELED;
            responses.push_back(std::move(response));
            continue;
        }
        responses.push_back(capture_with_curl(request, cancel));
    }
    return responses;
}

}  // namespace settings_http
