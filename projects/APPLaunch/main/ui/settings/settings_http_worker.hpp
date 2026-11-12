/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <string>
#include <vector>

namespace settings_http {

/**
 * One HTTP GET executed by the APPLaunch-side worker.
 *
 * `want_effective_url` is used for GitHub's `/releases/latest` redirect: the
 * body is still returned, and `Response::effective_url` contains the final URL
 * (for example `.../releases/tag/v1.0.3`).
 */
struct Request {
    std::string url;
    std::string accept_header;
    std::string user_agent = "APPLaunch-Software-Check";
    int timeout_ms = 15000;
    bool want_effective_url = false;
};

struct Response {
    bool ok = false;
    int code = -1;
    long status = 0;
    std::string body;
    std::string effective_url;
    std::string error;
};

using CancelFlag = std::atomic<bool>;

/**
 * Download a batch of URLs.
 *
 * When APPLaunch is compiled with libcurl (cross/device builds), a persistent
 * worker thread owns one CURLM handle and executes at most `max_concurrency`
 * transfers in parallel.  The calling thread must not be the LVGL thread.
 *
 * When libcurl is not compiled in, the function falls back to synchronous
 * `curl` subprocesses on the calling thread, preserving the previous behavior.
 * The fallback is bounded by `timeout_ms` (and the outer 30s timeout).
 *
 * `cancel` is checked by the libcurl progress callback and by the fallback
 * between requests; a cancelled batch returns entries with code -ECANCELED.
 */
std::vector<Response> download_batch(const std::vector<Request> &requests,
                                     const CancelFlag *cancel = nullptr,
                                     int max_concurrency = 6);

/** True when the libcurl multi worker is compiled in and running. */
bool libcurl_available();

/**
 * Wake the libcurl multi worker out of curl_multi_poll.  Safe to call from the
 * LVGL thread after setting the cancel flag; a no-op in the curl fallback.
 */
void wake();

}  // namespace settings_http
