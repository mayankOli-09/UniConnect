#pragma once
#include <memory>

#include "concurrency/DeadlockDetector.h"
#include "concurrency/Dispatcher.h"
#include "concurrency/SharedMemoryIPC.h"
#include "database/AcademicRepository.h"
#include "database/ChatRepository.h"
#include "database/NotificationRepository.h"
#include "database/UserRepository.h"
#include "http/Router.h"
#include "routes/WebSocketHandler.h"
#include "server/EpollServer.h"
#include "services/AcademicService.h"
#include "services/AuthService.h"
#include "services/ChatService.h"
#include "services/NotificationService.h"
#include "services/OsService.h"
#include "websocket/WebSocketManager.h"

namespace uniconnect {
namespace server {

struct ServerConfig {
    int port = 8080;
    size_t worker_threads = 4;                                          // thread-pool size
    size_t queue_capacity = 1000;                                       // tasks that may wait for a worker
    concurrency::SchedulingPolicy policy = concurrency::SchedulingPolicy::FCFS;
    int time_quantum_ms = 25;
};

// Storage the server works with. A null pointer leaves that feature's routes out.
struct ServerDeps {
    database::UserRepository* users = nullptr;          // required
    database::AcademicRepository* academics = nullptr;  // courses, hierarchy, enrollment
    database::ChatRepository* chat = nullptr;           // groups and messages
    database::NotificationRepository* notifications = nullptr;   // announcements and notifications (needs chat)
};

// The application server: owns the socket server, the router and the services.
class Server {
public:
    Server(ServerConfig config, ServerDeps deps,
           services::AuthConfig auth_config = services::AuthConfig::fromEnvironment());
    ~Server();

    void start();            // blocks until stop() is called
    void stop();
    bool isRunning() const;
    services::AuthService& auth() { return auth_; }
    services::ChatService* chat() { return chat_.get(); }
    services::NotificationService* notifications() { return notifications_.get(); }
    concurrency::Dispatcher& dispatcher() { return *dispatcher_; }
    services::OsService& os() { return *os_; }
    size_t websocketSessions() const { return ws_manager_->sessionCount(); }

private:
    void registerRoutes();
    void onHttpRequest(int fd, const http::HttpRequest& request);

    ServerConfig config_;
    ServerDeps deps_;
    services::AuthService auth_;
    std::unique_ptr<services::AcademicService> academics_;
    std::unique_ptr<services::ChatService> chat_;
    std::unique_ptr<services::NotificationService> notifications_;
    concurrency::DeadlockDetector deadlock_;
    concurrency::SharedMemoryIPC shared_memory_;
    std::unique_ptr<services::OsService> os_;
    http::Router router_;
    EpollServer socket_server_;
    std::unique_ptr<websocket::WebSocketManager> ws_manager_;
    std::unique_ptr<routes::WebSocketHandler> ws_handler_;
    std::unique_ptr<concurrency::Dispatcher> dispatcher_;   // declared last: stopped first, before the services its tasks use
};

} // namespace server
} // namespace uniconnect
