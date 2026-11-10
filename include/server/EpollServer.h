#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include "server/Connection.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "websocket/WebSocketParser.h"

namespace uniconnect {
namespace server {

using HttpRequestHandler = std::function<void(int client_fd, const http::HttpRequest& request)>;
using WsFrameHandler = std::function<void(int client_fd, const websocket::WsFrame& frame)>;
using DisconnectHandler = std::function<void(int client_fd)>;

class EpollServer {
public:
    explicit EpollServer(int port);
    ~EpollServer();

    void setHttpHandler(HttpRequestHandler handler);
    void setWsFrameHandler(WsFrameHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);

    void start();
    void stop();
    bool isRunning() const;

    // Thread-safe send
    void sendData(int client_fd, const std::string& data);
    void closeClient(int client_fd);

    size_t getActiveConnectionsCount() const;

private:
    int port_;
    int listen_fd_{-1};
    int epoll_fd_{-1};
    int wakeup_fd_{-1};
    std::atomic<bool> running_{false};

    mutable std::mutex connections_mutex_;
    std::map<int, std::shared_ptr<Connection>> connections_;

    HttpRequestHandler http_handler_;
    WsFrameHandler ws_handler_;
    DisconnectHandler disconnect_handler_;

    void setupListeningSocket();
    void setupEpoll();
    void runEventLoop();

    void handleAccept();
    void handleRead(int fd);
    void handleWrite(int fd);
    void removeConnection(int fd);

    bool setNonBlocking(int fd);
    void triggerWakeup();
};

} // namespace server
} // namespace uniconnect
