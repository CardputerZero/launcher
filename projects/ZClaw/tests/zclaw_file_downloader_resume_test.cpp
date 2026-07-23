#include "zclaw_file_downloader.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

namespace {

class OneShotServer {
public:
    explicit OneShotServer(std::function<std::string(const std::string &)> reply)
    {
        socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        assert(socket_ >= 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        assert(::bind(socket_, reinterpret_cast<sockaddr *>(&address),
                      sizeof(address)) == 0);
        assert(::listen(socket_, 1) == 0);
        socklen_t length = sizeof(address);
        assert(::getsockname(socket_, reinterpret_cast<sockaddr *>(&address),
                             &length) == 0);
        port_ = ntohs(address.sin_port);
        thread_ = std::thread([this, reply = std::move(reply)] {
            const int client = ::accept(socket_, nullptr, nullptr);
            assert(client >= 0);
            std::string request;
            char buffer[1024];
            while (request.find("\r\n\r\n") == std::string::npos) {
                const ssize_t count = ::read(client, buffer, sizeof(buffer));
                assert(count > 0);
                request.append(buffer, static_cast<std::size_t>(count));
            }
            const std::string response = reply(request);
            std::size_t offset = 0;
            while (offset < response.size()) {
                const ssize_t count = ::write(client, response.data() + offset,
                                              response.size() - offset);
                assert(count > 0);
                offset += static_cast<std::size_t>(count);
            }
            ::close(client);
        });
    }

    ~OneShotServer()
    {
        if (thread_.joinable())
            thread_.join();
        ::close(socket_);
    }

    std::string url() const
    {
        return "http://127.0.0.1:" + std::to_string(port_) + "/archive";
    }

private:
    int socket_ = -1;
    int port_ = 0;
    std::thread thread_;
};

void write_file(const std::string &path, const std::string &content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
    assert(output.good());
}

std::string read_file(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

}  // namespace

int main()
{
    const std::string output_path = "/tmp/zclaw-resume-test.part";
    std::string error;
    write_file(output_path, "hello");
    std::size_t final_bytes = 0;
    {
        OneShotServer server([](const std::string &request) {
            assert(request.find("Range: bytes=5-") != std::string::npos);
            return std::string(
                "HTTP/1.1 206 Partial Content\r\n"
                "Content-Range: bytes 5-10/11\r\n"
                "Content-Length: 6\r\nConnection: close\r\n\r\n world");
        });
        assert(zclaw::FileDownloader().download(
            server.url(), output_path, &error,
            [&final_bytes](const zclaw::DownloadProgress &progress) {
                final_bytes = progress.downloaded_bytes;
            }));
    }
    assert(error.empty());
    assert(read_file(output_path) == "hello world");
    assert(final_bytes == 11);

    write_file(output_path, "old partial");
    {
        OneShotServer server([](const std::string &request) {
            assert(request.find("Range: bytes=11-") != std::string::npos);
            return std::string(
                "HTTP/1.1 200 OK\r\nContent-Length: 11\r\n"
                "Connection: close\r\n\r\nreplacement");
        });
        assert(zclaw::FileDownloader().download(server.url(), output_path,
                                                 &error));
    }
    assert(read_file(output_path) == "replacement");

    write_file(output_path, "complete");
    {
        OneShotServer server([](const std::string &request) {
            assert(request.find("Range: bytes=8-") != std::string::npos);
            return std::string(
                "HTTP/1.1 416 Range Not Satisfiable\r\n"
                "Content-Range: bytes */8\r\nContent-Length: 0\r\n"
                "Connection: close\r\n\r\n");
        });
        assert(zclaw::FileDownloader().download(server.url(), output_path,
                                                 &error));
    }
    assert(read_file(output_path) == "complete");

    write_file(output_path, "hello");
    {
        OneShotServer server([](const std::string &) {
            return std::string(
                "HTTP/1.1 206 Partial Content\r\n"
                "Content-Range: bytes 5-10/11\r\n"
                "Content-Length: 6\r\nConnection: close\r\n\r\n wo");
        });
        assert(!zclaw::FileDownloader().download(server.url(), output_path,
                                                  &error));
    }
    assert(read_file(output_path) == "hello wo");
    ::unlink(output_path.c_str());
    return 0;
}
