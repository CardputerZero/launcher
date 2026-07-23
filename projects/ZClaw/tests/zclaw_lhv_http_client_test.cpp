#include "zclaw_lhv_http_client.h"

#include <arpa/inet.h>
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

class LocalServer {
public:
    explicit LocalServer(bool respond) : respond_(respond)
    {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        assert(fd_ >= 0);
        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        assert(::bind(fd_, reinterpret_cast<sockaddr *>(&address),
                      sizeof(address)) == 0);
        socklen_t length = sizeof(address);
        assert(::getsockname(fd_, reinterpret_cast<sockaddr *>(&address),
                             &length) == 0);
        port_ = ntohs(address.sin_port);
        assert(::listen(fd_, 1) == 0);
        thread_ = std::thread([this] { serve(); });
    }

    ~LocalServer()
    {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        if (thread_.joinable())
            thread_.join();
    }

    int port() const { return port_; }

    void wait_for_accept()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        assert(changed_.wait_for(lock, std::chrono::seconds(2),
                                 [this] { return accepted_; }));
    }

private:
    void serve()
    {
        const int client = ::accept(fd_, nullptr, nullptr);
        if (client < 0)
            return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            accepted_ = true;
        }
        changed_.notify_all();
        char request[1024];
        (void)::recv(client, request, sizeof(request), 0);
        if (respond_) {
            static constexpr char response[] =
                "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n"
                "Connection: close\r\n\r\nok";
            (void)::send(client, response, std::strlen(response), 0);
        } else {
            char byte = 0;
            (void)::recv(client, &byte, 1, 0);
        }
        ::close(client);
    }

    bool respond_;
    int fd_ = -1;
    int port_ = 0;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable changed_;
    bool accepted_ = false;
};

HttpRequestPtr request_for(int port)
{
    auto request = std::make_shared<HttpRequest>();
    request->method = HTTP_GET;
    request->url = "http://127.0.0.1:" + std::to_string(port) + "/test";
    request->timeout = 5;
    return request;
}

}  // namespace

int main()
{
    {
        LocalServer server(true);
        zclaw::LhvHttpClient client;
        std::promise<HttpResponsePtr> completed;
        assert(client.send(request_for(server.port()),
                           [&completed](const HttpResponsePtr &response) {
                               completed.set_value(response);
                           }));
        const HttpResponsePtr response = completed.get_future().get();
        assert(response && response->status_code == 200);
        assert(response->body == "ok");
    }

    {
        LocalServer server(false);
        zclaw::LhvHttpClient client;
        std::promise<HttpResponsePtr> completed;
        assert(client.send(request_for(server.port()),
                           [&completed](const HttpResponsePtr &response) {
                               completed.set_value(response);
                           }));
        std::future<HttpResponsePtr> result = completed.get_future();
        server.wait_for_accept();
        client.shutdown();
        assert(result.wait_for(std::chrono::seconds(2)) ==
               std::future_status::ready);
        assert(!result.get());
    }
    return 0;
}
