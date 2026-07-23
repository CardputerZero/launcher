#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace zclaw {

class HttpCancellationRegistry;
class LhvHttpClient;

struct DownloadProgress {
    std::size_t downloaded_bytes = 0;
    std::size_t total_bytes = 0;
    double bytes_per_second = 0.0;
};

class FileDownloader {
public:
    using ProgressHandler = std::function<void(const DownloadProgress &)>;

    FileDownloader();
    explicit FileDownloader(
        std::shared_ptr<HttpCancellationRegistry> cancellation);
    FileDownloader(std::shared_ptr<HttpCancellationRegistry> cancellation,
                   std::shared_ptr<LhvHttpClient> client);

    bool download(const std::string &url, const std::string &output_path,
                  std::string *error,
                  const ProgressHandler &progress_handler = {}) const;

private:
    std::shared_ptr<HttpCancellationRegistry> cancellation_;
    std::shared_ptr<LhvHttpClient> client_;
};

}  // namespace zclaw
