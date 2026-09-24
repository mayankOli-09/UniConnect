#include "server/EpollServer.h"
#include "http/HttpParser.h"
#include "utils/Logger.h"

#include <cstring>
#include <cerrno>

#if !defined(_WIN32)
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace uniconnect {
namespace server {

static const int MAX_EPOLL_EVENTS = 64;

EpollServer::EpollServer(int port) : port_(port) {}

EpollServer::~EpollServer() {
    stop();
}

void EpollServer::setHttpHandler(HttpRequestHandler handler) {
    http_handler_ = std::move(handler);
}

void EpollServer::setWsFrameHandler(WsFrameHandler handler) {
    ws_handler_ = std::move(handler);
}

void EpollServer::setDisconnectHandler(DisconnectHandler handler) {
    disconnect_handler_ = std::move(handler);
}

bool EpollServer::setNonBlocking(int fd) {
#if !defined(_WIN32)
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
#else
    return true;
#endif
}

void EpollServer::setupListeningSocket() {
#if !defined(_WIN32)
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        throw std::runtime_error("Failed to create listen socket: " + std::string(strerror(errno)));
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    setNonBlocking(listen_fd_);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        throw std::runtime_error("Failed to bind socket to port " + std::to_string(port_) + ": " + strerror(errno));
    }

    if (listen(listen_fd_, 1024) == -1) {
        throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
    }
#endif
    utils::Logger::getInstance().info("EpollServer", "Bound & listening on port", port_);
}

void EpollServer::setupEpoll() {
#if !defined(_WIN32)
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        throw std::runtime_error("Failed to create epoll instance: " + std::string(strerror(errno)));
    }

    wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeup_fd_ != -1) {
        epoll_event ev_wakeup{};
        ev_wakeup.events = EPOLLIN;
        ev_wakeup.data.fd = wakeup_fd_;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev_wakeup);
    }

    epoll_event ev_listen{};
    ev_listen.events = EPOLLIN | EPOLLET;
    ev_listen.data.fd = listen_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev_listen) == -1) {
        throw std::runtime_error("Failed to add listen_fd to epoll: " + std::string(strerror(errno)));
    }
#endif
}

void EpollServer::triggerWakeup() {
#if !defined(_WIN32)
    if (wakeup_fd_ != -1) {
        uint64_t val = 1;
        ssize_t w = write(wakeup_fd_, &val, sizeof(val));
        (void)w;
    }
#endif
}

void EpollServer::start() {
    setupListeningSocket();
    setupEpoll();
    running_ = true;
    utils::Logger::getInstance().info("EpollServer", "Starting custom epoll asynchronous event loop...");
    runEventLoop();
}

void EpollServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    triggerWakeup();

#if !defined(_WIN32)
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    if (wakeup_fd_ != -1) {
        close(wakeup_fd_);
        wakeup_fd_ = -1;
    }
    if (listen_fd_ != -1) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
#endif

    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.clear();
    utils::Logger::getInstance().info("EpollServer", "Stopped epoll event loop");
}

bool EpollServer::isRunning() const {
    return running_;
}

void EpollServer::handleAccept() {
#if !defined(_WIN32)
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // All pending connections accepted
            }
            break;
        }

        setNonBlocking(client_fd);

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = client_fd;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            close(client_fd);
            continue;
        }

        auto conn = std::make_shared<Connection>(client_fd);
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[client_fd] = conn;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);
        utils::Logger::getInstance().debug("EpollServer", "Accepted connection from", ip, "on fd:", client_fd);
    }
#endif
}

void EpollServer::handleRead(int fd) {
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;
        conn = it->second;
    }

    char buffer[4096];
    bool client_closed = false;

#if !defined(_WIN32)
    while (true) {
        ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
        if (bytes > 0) {
            conn->appendReadData(buffer, bytes);
        } else if (bytes == 0) {
            client_closed = true;
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // Read all currently available data
            }
            client_closed = true;
            break;
        }
    }
#endif

    if (client_closed) {
        removeConnection(fd);
        return;
    }

    // Process read buffer according to ConnectionType
    if (conn->getType() == ConnectionType::HTTP) {
        std::string& r_buf = conn->getReadBuffer();
        http::HttpRequest request;
        size_t consumed = 0;
        auto res = http::HttpParser::parse(r_buf, request, consumed);

        if (res == http::HttpParseResult::COMPLETE) {
            conn->consumeReadBuffer(consumed);
            if (http_handler_) {
                http_handler_(fd, request);
            }
        }
    } else if (conn->getType() == ConnectionType::WEBSOCKET) {
        std::string& r_buf = conn->getReadBuffer();
        while (!r_buf.empty()) {
            websocket::WsFrame frame;
            size_t consumed = 0;
            auto status = websocket::WebSocketParser::parseFrame(
                reinterpret_cast<const uint8_t*>(r_buf.data()),
                r_buf.size(),
                frame,
                consumed
            );

            if (status == websocket::WsParseStatus::FRAME_READY) {
                conn->consumeReadBuffer(consumed);
                if (ws_handler_) {
                    ws_handler_(fd, frame);
                }
            } else if (status == websocket::WsParseStatus::NEED_MORE_DATA) {
                break;
            } else {
                // Protocol error
                removeConnection(fd);
                break;
            }
        }
    }
}

void EpollServer::handleWrite(int fd) {
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;
        conn = it->second;
    }

    if (!conn->hasPendingWrite()) {
        return;
    }

    std::string data = conn->getPendingWriteData();
    size_t total_sent = 0;

#if !defined(_WIN32)
    while (total_sent < data.size()) {
        ssize_t sent = send(fd, data.data() + total_sent, data.size() - total_sent, 0);
        if (sent > 0) {
            total_sent += sent;
        } else if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // Socket send buffer full, wait for next EPOLLOUT
            }
            removeConnection(fd);
            return;
        } else {
            break;
        }
    }
#endif

    conn->consumeWriteBuffer(total_sent);

#if !defined(_WIN32)
    // If all written, remove EPOLLOUT to avoid busy spin
    if (!conn->hasPendingWrite()) {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }
#endif
}

void EpollServer::sendData(int client_fd, const std::string& data) {
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(client_fd);
        if (it == connections_.end()) return;
        conn = it->second;
    }

    conn->appendWriteData(data);

#if !defined(_WIN32)
    if (epoll_fd_ != -1) {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
    }
#endif
    triggerWakeup();
}

void EpollServer::removeConnection(int fd) {
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.erase(fd);
    }

#if !defined(_WIN32)
    if (epoll_fd_ != -1) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    }
#endif

    if (disconnect_handler_) {
        disconnect_handler_(fd);
    }
    utils::Logger::getInstance().debug("EpollServer", "Closed connection on fd:", fd);
}

void EpollServer::closeClient(int client_fd) {
    removeConnection(client_fd);
}

size_t EpollServer::getActiveConnectionsCount() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}

void EpollServer::runEventLoop() {
#if !defined(_WIN32)
    epoll_event events[MAX_EPOLL_EVENTS];

    while (running_) {
        int n_events = epoll_wait(epoll_fd_, events, MAX_EPOLL_EVENTS, 500);

        if (n_events == -1) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n_events; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == listen_fd_) {
                handleAccept();
            } else if (fd == wakeup_fd_) {
                uint64_t val = 0;
                ssize_t r = read(wakeup_fd_, &val, sizeof(val));
                (void)r;
            } else if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                removeConnection(fd);
            } else {
                if (ev & EPOLLIN) {
                    handleRead(fd);
                }
                if (ev & EPOLLOUT) {
                    handleWrite(fd);
                }
            }
        }
    }
#endif
}

} // namespace server
} // namespace uniconnect
