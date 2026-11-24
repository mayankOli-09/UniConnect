#include "server/Server.h"

#include <chrono>

#include <stdexcept>

#include "routes/AcademicRoutes.h"
#include "routes/AuthRoutes.h"
#include "routes/ChatRoutes.h"
#include "routes/NotificationRoutes.h"
#include "routes/OsRoutes.h"
#include "utils/Logger.h"

namespace uniconnect {
namespace server {

using http::HttpMethod;
using http::HttpRequest;
using http::HttpResponse;

namespace {
database::UserRepository& requireUsers(const ServerDeps& d) {
    if (!d.users) throw std::invalid_argument("Server needs a user repository");
    return *d.users;
}
} // namespace

Server::Server(ServerConfig config, ServerDeps deps, services::AuthConfig auth_config)
    : config_(config), deps_(deps), auth_(requireUsers(deps), std::move(auth_config)), socket_server_(config.port) {
    if (deps_.academics) academics_ = std::make_unique<services::AcademicService>(*deps_.academics);
    if (deps_.chat) chat_ = std::make_unique<services::ChatService>(*deps_.chat);

    if (deps_.notifications && deps_.chat) notifications_ = std::make_unique<services::NotificationService>(*deps_.notifications, *deps_.chat);

    concurrency::DispatcherConfig dc;
    dc.worker_threads = config.worker_threads;
    dc.queue_capacity = config.queue_capacity;
    dc.policy = config.policy;
    dc.time_quantum_ms = config.time_quantum_ms;
    dispatcher_ = std::make_unique<concurrency::Dispatcher>(dc);

    ws_manager_ = std::make_unique<websocket::WebSocketManager>([this](int fd, const std::string& bytes) { socket_server_.sendData(fd, bytes); });
    ws_handler_ = std::make_unique<routes::WebSocketHandler>(auth_, *deps_.users, chat_.get(), *ws_manager_, [this](int fd) { socket_server_.closeClient(fd); });
    socket_server_.setWsFrameHandler([this](int fd, const websocket::WsFrame& f) { ws_handler_->onFrame(fd, f); });
    socket_server_.setDisconnectHandler([this](int fd) { ws_handler_->onDisconnect(fd); });

    if (notifications_) {
        notifications_->setDispatcher(dispatcher_.get());
        // Every recipient who is online gets the notification immediately.
        notifications_->setListener([this](const std::vector<models::Notification>& made) {
            for (const auto& n : made) {
                auto event = utils::JsonValue::object();
                event["event"] = "notification";
                event["data"] = n.toJson();
                ws_manager_->broadcastToUsers({n.user_id}, event.serialize(false));
            }
        });
    }
    os_ = std::make_unique<services::OsService>(*dispatcher_, deadlock_, shared_memory_,
                                                [this]() { return socket_server_.getActiveConnectionsCount(); },
                                                [this]() { return ws_manager_->sessionCount(); });
    if (chat_) {
        chat_->setDispatcher(dispatcher_.get());
        chat_->setOnlineChecker([this](int id) { return ws_manager_->isOnline(id); });
        // A new direct chat or group appears in the other person's list at once.
        chat_->setGroupAddedListener([this](int userId, const models::ChatGroup& g) {
            auto event = utils::JsonValue::object();
            event["event"] = "group_added";
            event["data"] = g.toJson();
            ws_manager_->broadcastToUsers({userId}, event.serialize(false));
        });
        // A stored message is pushed to every group member who has the group open.
        chat_->setMessageListener([this](const models::ChatMessage& m, const std::vector<int>& recipients) {
            auto event = utils::JsonValue::object();
            event["event"] = "new_message";
            event["data"] = m.toJson();
            ws_manager_->broadcastToGroup(m.group_id, event.serialize(false), recipients);
        });
    }
    registerRoutes();
    socket_server_.setHttpHandler([this](int fd, const HttpRequest& req) { onHttpRequest(fd, req); });
}

void Server::registerRoutes() {
    router_.get("/health", [](const HttpRequest&, const http::RouteParams&) {
        auto o = utils::JsonValue::object();
        o["status"] = "ok";
        o["service"] = "UniConnect Backend";
        return HttpResponse::okJson(o);
    });
    routes::registerAuthRoutes(router_, auth_);
    if (academics_) routes::registerAcademicRoutes(router_, *academics_, auth_, *deps_.users);
    if (chat_) routes::registerChatRoutes(router_, *chat_, auth_, *deps_.users);
    if (notifications_) routes::registerNotificationRoutes(router_, *notifications_, auth_, *deps_.users);
    routes::registerOsRoutes(router_, *os_, auth_, *deps_.users);
}

void Server::onHttpRequest(int fd, const HttpRequest& req) {
    const auto started = std::chrono::steady_clock::now();
    HttpResponse res;

    if (req.isWebSocketUpgrade()) {   // GET /ws with Upgrade: websocket
        bool accepted = false;
        if (req.path == "/ws") res = ws_handler_->handshake(fd, req, accepted);
        else res = HttpResponse::notFound("Endpoint not found: " + req.path);
        socket_server_.sendData(fd, res.toString());
        if (!accepted) socket_server_.closeClient(fd);   // otherwise the socket server switches this connection to WebSocket mode
        utils::Logger::getInstance().info("HTTP", "WebSocket upgrade", req.path, res.status_code);
        return;
    }

    if (req.method == HttpMethod::OPTIONS) {
        res = HttpResponse::corsOptions();           // CORS preflight
    } else {
        const auto m = router_.match(req.method, req.path);
        switch (m.kind) {
            case http::Router::Match::Kind::FOUND:
                try {
                    res = m.handler(req, m.params);
                } catch (const std::exception& e) {
                    utils::Logger::getInstance().error("HTTP", "Handler failed for", req.path, ":", e.what());
                    res = HttpResponse::serverError("Internal server error");
                }
                break;
            case http::Router::Match::Kind::METHOD_NOT_ALLOWED:
                res = HttpResponse::errorJson(405, "Method Not Allowed", "Method not allowed for " + req.path);
                res.setHeader("Allow", m.allow);
                break;
            default:
                res = HttpResponse::notFound("Endpoint not found: " + req.path);
        }
    }

    socket_server_.sendData(fd, res.toString());

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    auto& log = utils::Logger::getInstance();
    if (req.path == "/health") log.debug("HTTP", req.method_text, req.path, res.status_code, ms, "ms");
    else log.info("HTTP", req.method_text, req.path, res.status_code, ms, "ms");   // never the body: it may hold passwords
}

Server::~Server() { dispatcher_->stop(); }

void Server::start() {
    if (!shared_memory_.initialize(true)) utils::Logger::getInstance().warn("Server", "Shared-memory telemetry is unavailable");
    dispatcher_->start();
    os_->updateTelemetry();
    socket_server_.start();
    dispatcher_->stop();
    shared_memory_.cleanup();
}
void Server::stop() { socket_server_.stop(); }
bool Server::isRunning() const { return socket_server_.isRunning(); }

} // namespace server
} // namespace uniconnect
