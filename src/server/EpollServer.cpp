// Windows headers must come first.
#include <winsock2.h>
#include <ws2tcpip.h>

#include "server/EpollServer.h"
#include "utils/Logger.h"

#include <stdexcept>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#endif

namespace uniconnect {
namespace server {

namespace {
struct WsaInit {
    WsaInit() {
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d) != 0) throw std::runtime_error("WSAStartup failed");
    }
    ~WsaInit() { WSACleanup(); }
};
inline bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
inline std::string wsaError() { return "Winsock error " + std::to_string(WSAGetLastError()); }
inline SOCKET asSock(int fd) { return static_cast<SOCKET>(fd); }
} // namespace

EpollServer::EpollServer(int port) : port_(port) {}

EpollServer::~EpollServer() {
    stop_requested_ = true;
    if (listen_fd_ != -1) { closesocket(asSock(listen_fd_)); listen_fd_ = -1; }
}

void EpollServer::setupListeningSocket() {
    static WsaInit wsa;  // WSAStartup once per process

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) throw std::runtime_error("Failed to create socket: " + wsaError());
    listen_fd_ = static_cast<int>(s);

    // On Windows SO_REUSEADDR lets another program steal the port; ask for exclusive use instead.
    BOOL excl = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&excl), sizeof(excl));

    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<u_short>(port_));
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        const std::string e = wsaError();
        closesocket(s); listen_fd_ = -1;
        throw std::runtime_error("Cannot bind port " + std::to_string(port_) + " (" + e + "). Is another program using it?");
    }
    if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
        const std::string e = wsaError();
        closesocket(s); listen_fd_ = -1;
        throw std::runtime_error("listen() failed: " + e);
    }
}

void EpollServer::start() {
    stop_requested_ = false;
    setupListeningSocket();
    running_ = true;
    utils::Logger::getInstance().info("Server", "Listening on port", port_);
    runEventLoop();

    // shut down
    if (listen_fd_ != -1) { closesocket(asSock(listen_fd_)); listen_fd_ = -1; }
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.clear();   // Connection destructors close the sockets
    }
    running_ = false;
    utils::Logger::getInstance().info("Server", "Stopped");
}

void EpollServer::stop() { stop_requested_ = true; }

std::shared_ptr<Connection> EpollServer::find(int fd) const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(fd);
    return it == connections_.end() ? nullptr : it->second;
}

size_t EpollServer::getActiveConnectionsCount() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}

void EpollServer::handleAccept() {
    while (true) {
        sockaddr_in ca{};
        int len = sizeof(ca);
        SOCKET cs = accept(asSock(listen_fd_), reinterpret_cast<sockaddr*>(&ca), &len);
        if (cs == INVALID_SOCKET) break;   // WSAEWOULDBLOCK: nothing more waiting

        u_long nonblock = 1;
        ioctlsocket(cs, FIONBIO, &nonblock);
        BOOL nodelay = TRUE;
        setsockopt(cs, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

        const int fd = static_cast<int>(cs);
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[fd] = std::make_shared<Connection>(fd);
        }
        utils::Logger::getInstance().debug("Server", "Accepted connection, fd", fd);
    }
}

void EpollServer::handleRead(int fd) {
    auto conn = find(fd);
    if (!conn) return;

    char buf[8192];
    bool closed = false;
    while (true) {
        const int n = recv(asSock(fd), buf, static_cast<int>(sizeof(buf)), 0);
        if (n > 0) { conn->appendReadData(buf, static_cast<size_t>(n)); continue; }
        if (n == 0) { closed = true; break; }          // peer closed
        if (wouldBlock()) break;                       // everything available has been read
        closed = true; break;                          // reset / error
    }
    if (closed) { removeConnection(fd); return; }

    // --- HTTP: handle every complete request in the buffer (keep-alive + pipelining)
    while (conn->getType() == ConnectionType::HTTP) {
        const std::string data = conn->peekReadBuffer();
        if (data.empty()) break;

        http::HttpRequest request;
        size_t consumed = 0;
        const auto res = http::HttpParser::parse(data, request, consumed);
        if (res == http::HttpParseResult::INCOMPLETE) break;
        if (res == http::HttpParseResult::INVALID) {
            conn->appendWriteData(http::HttpResponse::badRequest("Malformed or oversized HTTP request").toString());
            conn->setCloseAfterWrite();
            conn->consumeReadBuffer(data.size());
            break;
        }
        conn->consumeReadBuffer(consumed);

        const bool upgrade = request.isWebSocketUpgrade();
        if (http_handler_) {
            try {
                http_handler_(fd, request);
            } catch (const std::exception& e) {
                utils::Logger::getInstance().error("Server", "Unhandled error in", request.path, ":", e.what());
                conn->appendWriteData(http::HttpResponse::serverError("Internal server error").toString());
            }
        }
        // A handshake that was not rejected (the handler did not close the client) turns this
        // connection into a WebSocket connection.
        if (upgrade && find(fd) && !conn->closeAfterWrite()) conn->setType(ConnectionType::WEBSOCKET);
    }

    // --- WebSocket frames
    if (conn->getType() == ConnectionType::WEBSOCKET) {
        const std::string data = conn->peekReadBuffer();
        size_t pos = 0;
        while (pos < data.size()) {
            websocket::WsFrame frame;
            size_t used = 0;
            const auto st = websocket::WebSocketParser::parseFrame(
                reinterpret_cast<const uint8_t*>(data.data()) + pos, data.size() - pos, frame, used);
            if (st == websocket::WsParseStatus::NEED_MORE_DATA) break;
            if (st == websocket::WsParseStatus::INVALID) { removeConnection(fd); return; }
            pos += used;
            if (ws_handler_) {
                try { ws_handler_(fd, frame); }
                catch (const std::exception& e) { utils::Logger::getInstance().error("Server", "WebSocket handler error:", e.what()); }
            }
            if (!find(fd)) return;   // handler closed it
        }
        if (pos > 0) conn->consumeReadBuffer(pos);
    }

    handleWrite(fd);   // flush replies right away
}

void EpollServer::handleWrite(int fd) {
    auto conn = find(fd);
    if (!conn) return;

    if (conn->hasPendingWrite()) {
        const std::string data = conn->getPendingWriteData();
        size_t total = 0;
        while (total < data.size()) {
            const int n = send(asSock(fd), data.data() + total, static_cast<int>(data.size() - total), 0);
            if (n > 0) { total += static_cast<size_t>(n); continue; }
            if (n == SOCKET_ERROR && wouldBlock()) break;   // socket buffer full; retry on next writable event
            removeConnection(fd);
            return;
        }
        conn->consumeWriteBuffer(total);
    }
    if (conn->closeAfterWrite() && !conn->hasPendingWrite()) removeConnection(fd);
}

void EpollServer::sendData(int fd, const std::string& data) {
    auto conn = find(fd);
    if (conn) conn->appendWriteData(data);   // the event loop notices within one tick
}

void EpollServer::closeClient(int fd) {
    auto conn = find(fd);
    if (!conn) return;
    conn->setCloseAfterWrite();
    if (!conn->hasPendingWrite()) removeConnection(fd);
}

void EpollServer::removeConnection(int fd) {
    bool existed;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        existed = connections_.erase(fd) > 0;   // the socket closes when the last reference goes away
    }
    if (existed) {
        if (disconnect_handler_) disconnect_handler_(fd);
        utils::Logger::getInstance().debug("Server", "Closed connection, fd", fd);
    }
}

void EpollServer::runEventLoop() {
    std::vector<WSAPOLLFD> fds;
    while (!stop_requested_) {
        fds.clear();
        fds.push_back({asSock(listen_fd_), POLLRDNORM, 0});
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            for (const auto& [fd, conn] : connections_) {
                SHORT ev = POLLRDNORM;
                if (conn->hasPendingWrite()) ev |= POLLWRNORM;
                fds.push_back({asSock(fd), ev, 0});
            }
        }

        const int n = WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), 20);   // 20 ms tick
        if (n == SOCKET_ERROR) { Sleep(5); continue; }
        if (n == 0) continue;

        if (fds[0].revents & POLLRDNORM) handleAccept();

        for (size_t i = 1; i < fds.size(); ++i) {
            const int fd = static_cast<int>(fds[i].fd);
            const SHORT re = fds[i].revents;
            if (re == 0) continue;
            if (re & POLLRDNORM) handleRead(fd);           // also detects a graceful close
            else if (re & (POLLERR | POLLHUP | POLLNVAL)) { removeConnection(fd); continue; }
            if (re & POLLWRNORM) handleWrite(fd);
        }
    }
}

} // namespace server
} // namespace uniconnect
