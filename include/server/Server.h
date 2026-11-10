#pragma once

#include <memory>
#include <string>
#include "server/EpollServer.h"
#include "concurrency/ThreadPool.h"
#include "concurrency/Scheduler.h"
#include "concurrency/TaskQueue.h"
#include "concurrency/DeadlockDetector.h"
#include "concurrency/SharedMemoryIPC.h"
#include "database/Database.h"
#include "database/UserRepository.h"
#include "database/CourseRepository.h"
#include "database/EnrollmentRepository.h"
#include "database/MessageRepository.h"
#include "services/AuthService.h"
#include "services/UserService.h"
#include "services/CourseService.h"
#include "services/ChatService.h"
#include "services/NotificationService.h"

namespace uniconnect {
namespace server {

class Server {
public:
    explicit Server(int port = 8080,
                    size_t thread_pool_size = 4,
                    concurrency::SchedulingPolicy policy = concurrency::SchedulingPolicy::FCFS);
    ~Server();

    void start();
    void stop();

    // OS Concurrency & Resource Engine accessors
    std::shared_ptr<concurrency::ThreadPool> getThreadPool() const { return thread_pool_; }
    std::shared_ptr<concurrency::Scheduler> getScheduler() const { return scheduler_; }
    std::shared_ptr<concurrency::TaskQueue> getTaskQueue() const { return task_queue_; }
    std::shared_ptr<concurrency::DeadlockDetector> getDeadlockDetector() const { return deadlock_detector_; }

    // Business Services accessors
    services::AuthService& getAuthService() { return *auth_service_; }
    services::CourseService& getCourseService() { return *course_service_; }
    services::ChatService& getChatService() { return *chat_service_; }
    services::NotificationService& getNotificationService() { return *notification_service_; }

private:
    int port_;
    std::unique_ptr<EpollServer> epoll_server_;

    // OS Concurrency Engine
    std::shared_ptr<concurrency::Scheduler> scheduler_;
    std::shared_ptr<concurrency::TaskQueue> task_queue_;
    std::shared_ptr<concurrency::ThreadPool> thread_pool_;
    std::shared_ptr<concurrency::DeadlockDetector> deadlock_detector_;
    std::unique_ptr<concurrency::SharedMemoryIPC> shm_ipc_;

    // Database & Repositories
    database::Database& db_;
    std::unique_ptr<database::UserRepository> user_repo_;
    std::unique_ptr<database::CourseRepository> course_repo_;
    std::unique_ptr<database::EnrollmentRepository> enroll_repo_;
    std::unique_ptr<database::MessageRepository> msg_repo_;

    // Services
    std::unique_ptr<services::AuthService> auth_service_;
    std::unique_ptr<services::UserService> user_service_;
    std::unique_ptr<services::CourseService> course_service_;
    std::unique_ptr<services::ChatService> chat_service_;
    std::unique_ptr<services::NotificationService> notification_service_;

    void setupRoutes();
    void handleHttpRequest(int client_fd, const http::HttpRequest& request);
    void handleWsFrame(int client_fd, const websocket::WsFrame& frame);
    void handleDisconnect(int client_fd);

    void updateSharedMemoryTelemetry();
};

} // namespace server
} // namespace uniconnect

// Alias for backwards compatibility with main.cpp
using Server = uniconnect::server::Server;