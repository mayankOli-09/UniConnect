#pragma once
// Windows socket server. The class keeps the name "EpollServer" so the rest of the code base
// (Server.cpp) does not change, but the event loop is WSAPoll based.
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "http/HttpParser.h"
#include "server/Connection.h"
#include "websocket/WebSocket.h"

namespace uniconnect {
namespace server {

class EpollServer {
public:
    using HttpRequestHandler = std::function<void(int, const http::HttpRequest&)>;
    using WsFrameHandler = std::function<void(int, const websocket::WsFrame&)>;
    using DisconnectHandler = std::function<void(int)>;

    explicit EpollServer(int port);
    ~EpollServer();

    void setHttpHandler(HttpRequestHandler h) { http_handler_ = std::move(h); }
    void setWsFrameHandler(WsFrameHandler h) { ws_handler_ = std::move(h); }
    void setDisconnectHandler(DisconnectHandler h) { disconnect_handler_ = std::move(h); }

    // Blocks, running the event loop, until stop() is called. Throws if the port cannot be bound.
    void start();
    void stop();
    bool isRunning() const { return running_; }

    // Thread-safe: queue bytes for a client (may be called from worker threads).
    void sendData(int fd, const std::string& data);
    // Close a client; anything already queued for it is flushed first.
    void closeClient(int fd);
    size_t getActiveConnectionsCount() const;

private:
    void setupListeningSocket();
    void runEventLoop();
    void handleAccept();
    void handleRead(int fd);
    void handleWrite(int fd);
    void removeConnection(int fd);
    std::shared_ptr<Connection> find(int fd) const;

    int port_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    mutable std::mutex connections_mutex_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;

    HttpRequestHandler http_handler_;
    WsFrameHandler ws_handler_;
    DisconnectHandler disconnect_handler_;
};

} // namespace server
} // namespace uniconnect
