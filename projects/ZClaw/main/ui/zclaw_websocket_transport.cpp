#include "zclaw_websocket_transport.h"

#include "zclaw_chat_event.h"
#include "zclaw_chat_stream_model.h"
#include "zclaw_http_cancellation.h"
#include "zclaw_protocol.h"

#include <hv/WebSocketClient.h>
#include <hv/hasync.h>

#include <memory>
#include <mutex>
#include <utility>

namespace zclaw {
namespace {

class WebSocketOperation : public std::enable_shared_from_this<WebSocketOperation> {
public:
    WebSocketOperation(UiConfig config,
                       WebSocketTransport::ApprovalHandler approval,
                       WebSocketTransport::Completion completion)
        : config_(std::move(config)), approval_(std::move(approval)),
          completion_(std::move(completion)),
          socket_(std::make_shared<hv::WebSocketClient>())
    {
    }

    std::shared_ptr<hv::WebSocketClient> socket() const
    {
        return socket_;
    }

    void opened(const std::string &message)
    {
        const auto socket = active_socket();
        if (socket &&
            socket->send(protocol::make_ws_chat_message(message)) < 0)
            finish("WS chat failed.\nCould not send message.");
    }

    void message(const std::string &event_json)
    {
        const auto socket = active_socket();
        if (!socket || socket->opcode() != WS_OPCODE_TEXT || event_json.empty())
            return;
        const ChatStreamUpdate update = stream_.accept(parse_chat_event(event_json));
        if (update.approval_requested) {
            const std::weak_ptr<WebSocketOperation> weak = shared_from_this();
            if (approval_) {
                approval_(update.approval, [weak, request_id = update.approval.request_id](
                                               std::string decision) {
                    if (const auto operation = weak.lock())
                        operation->send_approval(request_id, std::move(decision));
                });
            } else {
                send_approval(update.approval.request_id, "deny");
            }
        }
        if (update.finished)
            finish_stream();
    }

    void closed()
    {
        if (stream_.finished())
            finish_stream();
        else if (stream_.partial_response().empty())
            finish("WS chat failed.\nConnection closed before completion.");
        else
            finish("WS chat interrupted.\n" + stream_.partial_response());
    }

    void cancel()
    {
        finish("WS chat failed.\nRequest cancelled.");
    }

    void timeout()
    {
        finish("WS chat failed.\nRequest timed out.");
    }

    HttpCancellationRegistration registration;

private:
    std::shared_ptr<hv::WebSocketClient> active_socket()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return finished_ ? nullptr : socket_;
    }

    void send_approval(const std::string &request_id, std::string decision)
    {
        bool failed = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (finished_)
                return;
            failed = socket_->send(protocol::make_ws_approval_response(
                         request_id, decision.empty() ? "deny" : decision)) < 0;
        }
        if (failed)
            finish("WS chat failed.\nCould not send approval.");
    }

    void finish_stream()
    {
        OperationResult result;
        result.config = config_;
        result.text = stream_.response().empty()
                          ? "ZeroClaw returned an empty WS response."
                          : stream_.response();
        result.ok = stream_.succeeded();
        finish(std::move(result));
    }

    void finish(std::string text)
    {
        finish({std::move(text), false, config_});
    }

    void finish(OperationResult result)
    {
        std::shared_ptr<hv::WebSocketClient> socket;
        WebSocketTransport::Completion completion;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (finished_)
                return;
            finished_ = true;
            socket = std::move(socket_);
            completion = std::move(completion_);
        }
        registration.reset();
        socket->onopen = nullptr;
        socket->onmessage = nullptr;
        socket->onclose = nullptr;
        socket->close();
        (void)hv::async([socket = std::move(socket),
                         completion = std::move(completion),
                         result = std::move(result)]() mutable {
            socket->stop();
            completion(std::move(result));
        });
    }

    UiConfig config_;
    WebSocketTransport::ApprovalHandler approval_;
    WebSocketTransport::Completion completion_;
    std::shared_ptr<hv::WebSocketClient> socket_;
    ChatStreamState stream_;
    std::mutex mutex_;
    bool finished_ = false;
};

}  // namespace

WebSocketTransport::WebSocketTransport(
    std::shared_ptr<HttpCancellationRegistry> cancellation)
    : cancellation_(std::move(cancellation))
{
}

bool WebSocketTransport::send(const UiConfig &config,
                              const std::string &message,
                              ApprovalHandler approval_handler,
                              Completion completion) const
{
    if (!completion)
        return false;
    const std::string websocket_url = protocol::gateway_ws_url(config);
    if (websocket_url.rfind("ws://", 0) != 0 &&
        websocket_url.rfind("wss://", 0) != 0) {
        completion({"WS chat failed.\nInvalid WebSocket URL.", false, config});
        return true;
    }

    auto operation = std::make_shared<WebSocketOperation>(
        config, std::move(approval_handler), std::move(completion));
    if (cancellation_) {
        const std::weak_ptr<WebSocketOperation> weak = operation;
        operation->registration = cancellation_->register_request([weak] {
            if (const auto operation = weak.lock())
                operation->cancel();
        });
        if (!operation->registration.active()) {
            operation->cancel();
            return true;
        }
    }

    const auto socket = operation->socket();
    socket->setConnectTimeout(15000);
    socket->setPingInterval(30000);
    socket->onopen = [operation, message] { operation->opened(message); };
    socket->onmessage = [operation](const std::string &event_json) {
        operation->message(event_json);
    };
    socket->onclose = [operation] { operation->closed(); };
    http_headers headers = {{"Sec-WebSocket-Protocol", "zeroclaw.v1"}};
    if (socket->open(websocket_url.c_str(), headers) != 0) {
        operation->closed();
        return true;
    }
    const std::weak_ptr<WebSocketOperation> weak = operation;
    socket->loop()->setTimeout(900 * 1000, [weak](auto) {
        if (const auto operation = weak.lock())
            operation->timeout();
    });
    return true;
}

}  // namespace zclaw
