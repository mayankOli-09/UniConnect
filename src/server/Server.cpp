#include "server/Server.h"
#include "websocket/WebSocket.h"
#include "utils/Logger.h"
#include <iostream>

namespace uniconnect {
namespace server {

Server::Server(int port, size_t thread_pool_size, concurrency::SchedulingPolicy policy)
    : port_(port),
      db_(database::Database::getInstance()) {

    // 1. Initialize OS Concurrency Engine
    scheduler_ = std::make_shared<concurrency::Scheduler>(policy, 25);
    task_queue_ = std::make_shared<concurrency::TaskQueue>(1000);
    thread_pool_ = std::make_shared<concurrency::ThreadPool>(thread_pool_size, scheduler_, task_queue_);
    deadlock_detector_ = std::make_shared<concurrency::DeadlockDetector>();
    shm_ipc_ = std::make_unique<concurrency::SharedMemoryIPC>("/uniconnect_os_telemetry");
    shm_ipc_->initialize(true);

    // 2. Initialize Repositories
    user_repo_ = std::make_unique<database::UserRepository>(db_);
    course_repo_ = std::make_unique<database::CourseRepository>(db_);
    enroll_repo_ = std::make_unique<database::EnrollmentRepository>(db_);
    msg_repo_ = std::make_unique<database::MessageRepository>(db_);

    // 3. Initialize Services
    auth_service_ = std::make_unique<services::AuthService>(*user_repo_);
    user_service_ = std::make_unique<services::UserService>(*user_repo_);
    course_service_ = std::make_unique<services::CourseService>(*course_repo_, *enroll_repo_);
    chat_service_ = std::make_unique<services::ChatService>(*msg_repo_, *enroll_repo_, thread_pool_);
    notification_service_ = std::make_unique<services::NotificationService>(*msg_repo_, *enroll_repo_, thread_pool_);

    // 4. Initialize Network Layer
    epoll_server_ = std::make_unique<EpollServer>(port_);
    setupRoutes();
}

Server::~Server() {
    stop();
}

void Server::setupRoutes() {
    // Connect WebSocketManager send callback to EpollServer sendData
    websocket::WebSocketManager::getInstance().setSendCallback(
        [this](int fd, const std::string& data) {
            epoll_server_->sendData(fd, data);
        }
    );

    // Wire EpollServer callbacks
    epoll_server_->setHttpHandler([this](int fd, const http::HttpRequest& req) {
        handleHttpRequest(fd, req);
    });

    epoll_server_->setWsFrameHandler([this](int fd, const websocket::WsFrame& frame) {
        handleWsFrame(fd, frame);
    });

    epoll_server_->setDisconnectHandler([this](int fd) {
        handleDisconnect(fd);
    });
}

void Server::start() {
    utils::Logger::getInstance().info("Server", "Starting UniConnect C++20 Backend on port", port_);
    thread_pool_->start();
    updateSharedMemoryTelemetry();
    epoll_server_->start();
}

void Server::stop() {
    utils::Logger::getInstance().info("Server", "Shutting down UniConnect server...");
    if (epoll_server_) epoll_server_->stop();
    if (thread_pool_) thread_pool_->stop();
    if (shm_ipc_) shm_ipc_->cleanup();
}

void Server::updateSharedMemoryTelemetry() {
    concurrency::SharedMetricsSegment seg{};
    seg.magic = 0x554E4943;
    seg.version = 1;
    seg.active_connections = static_cast<uint32_t>(epoll_server_ ? epoll_server_->getActiveConnectionsCount() : 0);
    seg.worker_threads_total = static_cast<uint32_t>(thread_pool_->threadCount());

    auto worker_stats = thread_pool_->getWorkersStats();
    uint32_t busy = 0;
    for (const auto& w : worker_stats) {
        if (w.state == concurrency::WorkerState::BUSY) busy++;
    }
    seg.worker_threads_busy = busy;
    seg.queue_depth = static_cast<uint32_t>(task_queue_->size());
    seg.queue_capacity = static_cast<uint32_t>(task_queue_->capacity());
    seg.scheduler_policy = static_cast<uint32_t>(scheduler_->getPolicy());

    auto sm = scheduler_->getMetrics();
    seg.context_switches = sm.context_switches;
    seg.total_messages_processed = sm.total_tasks_completed;
    seg.avg_waiting_time_ms = sm.avg_waiting_time_ms;
    seg.avg_turnaround_time_ms = sm.avg_turnaround_time_ms;

    auto dl = deadlock_detector_->detectDeadlock();
    seg.deadlock_detected = dl.has_deadlock ? 1 : 0;

    shm_ipc_->update(seg);
}

void Server::handleHttpRequest(int client_fd, const http::HttpRequest& request) {
    // 1. WebSocket Upgrade Handshake
    if (request.isWebSocketUpgrade()) {
        http::HttpResponse handshake_resp;
        if (websocket::WebSocketManager::getInstance().handleHandshake(request, handshake_resp)) {
            // Upgrade connection state
            epoll_server_->sendData(client_fd, handshake_resp.toString());
            websocket::WebSocketManager::getInstance().addSession(client_fd);
            return;
        } else {
            epoll_server_->sendData(client_fd, handshake_resp.toString());
            epoll_server_->closeClient(client_fd);
            return;
        }
    }

    // 2. CORS Preflight
    if (request.method == http::HttpMethod::OPTIONS) {
        epoll_server_->sendData(client_fd, http::HttpResponse::corsOptions().toString());
        return;
    }

    http::HttpResponse response;
    const std::string& path = request.path;

    // --- Health Check ---
    if (path == "/health" || path == "/") {
        auto obj = utils::JsonValue::object();
        obj["status"] = "ok";
        obj["service"] = "UniConnect Backend";
        obj["session"] = "Graphic Era University 2026-27 (Team Avengers)";
        obj["active_policy"] = concurrency::schedulingPolicyToString(scheduler_->getPolicy());
        response = http::HttpResponse::okJson(obj);
    }
    // --- OS Live Dashboard Stats (Recharts Integration) ---
    else if (path == "/api/os/stats") {
        updateSharedMemoryTelemetry();
        auto root = utils::JsonValue::object();
        root["thread_pool"] = thread_pool_->getMetricsJson();
        root["scheduler"] = scheduler_->getMetrics().toJson();
        root["queue"] = task_queue_->getMetrics().toJson();
        root["deadlock"] = deadlock_detector_->detectDeadlock().toJson();
        root["deadlock_graph"] = deadlock_detector_->getGraphJson();
        root["shared_memory"] = shm_ipc_->readAsJson();
        root["active_connections"] = epoll_server_->getActiveConnectionsCount();
        root["ws_sessions"] = websocket::WebSocketManager::getInstance().activeSessionsCount();
        response = http::HttpResponse::okJson(root);
    }
    // --- OS CPU Scheduling Policy Switch ---
    else if (path == "/api/os/scheduler" && request.method == http::HttpMethod::POST) {
        auto json = request.getJsonBody();
        std::string policy_str = json["policy"].asString();
        auto new_policy = concurrency::stringToSchedulingPolicy(policy_str);
        scheduler_->setPolicy(new_policy);

        if (json.contains("time_quantum_ms")) {
            scheduler_->setTimeQuantum(json["time_quantum_ms"].asInt(25));
        }

        auto resp_obj = utils::JsonValue::object();
        resp_obj["success"] = true;
        resp_obj["new_policy"] = concurrency::schedulingPolicyToString(new_policy);
        resp_obj["time_quantum_ms"] = scheduler_->getTimeQuantum();
        response = http::HttpResponse::okJson(resp_obj);
    }
    // --- OS Deadlock Detection Test Injection ---
    else if (path == "/api/os/deadlock-test" && request.method == http::HttpMethod::POST) {
        auto json = request.getJsonBody();
        std::string action = json["action"].asString("create_cycle");

        if (action == "create_cycle") {
            deadlock_detector_->clear();
            // Create circular wait: T1 holds R1, wants R2. T2 holds R2, wants R1.
            deadlock_detector_->grantResource("WorkerThread-1", "Resource-MutexA");
            deadlock_detector_->requestResource("WorkerThread-1", "Resource-MutexB");
            deadlock_detector_->grantResource("WorkerThread-2", "Resource-MutexB");
            deadlock_detector_->requestResource("WorkerThread-2", "Resource-MutexA");
        } else {
            deadlock_detector_->clear();
        }

        auto report = deadlock_detector_->detectDeadlock();
        response = http::HttpResponse::okJson(report.toJson());
    }
    // --- Auth: Login ---
    else if (path == "/api/auth/login" && request.method == http::HttpMethod::POST) {
        auto json = request.getJsonBody();
        std::string email = json["email"].asString();
        std::string password = json["password"].asString();
        auto auth_res = auth_service_->login(email, password);
        if (auth_res.success) {
            response = http::HttpResponse::okJson(auth_res.toJson());
        } else {
            response = http::HttpResponse::unauthorized(auth_res.error_message);
        }
    }
    // --- Auth: Register ---
    else if (path == "/api/auth/register" && request.method == http::HttpMethod::POST) {
        auto json = request.getJsonBody();
        std::string name = json["name"].asString();
        std::string email = json["email"].asString();
        std::string password = json["password"].asString();
        std::string role_str = json["role"].asString("STUDENT");
        std::string dept_id = json["department_id"].asString("dept-cse-01");

        auto auth_res = auth_service_->registerUser(name, email, password, models::stringToUserRole(role_str), dept_id);
        if (auth_res.success) {
            response = http::HttpResponse::created(auth_res.toJson());
        } else {
            response = http::HttpResponse::badRequest(auth_res.error_message);
        }
    }
    // --- Academic Hierarchy Tree ---
    else if (path == "/api/hierarchy" && request.method == http::HttpMethod::GET) {
        response = http::HttpResponse::okJson(course_service_->getHierarchyTree());
    }
    // --- Courses List ---
    else if (path == "/api/courses" && request.method == http::HttpMethod::GET) {
        auto courses = course_service_->getAllCourses();
        auto arr = utils::JsonValue::array();
        for (const auto& c : courses) arr.push_back(c.toJson());
        auto root = utils::JsonValue::object();
        root["courses"] = arr;
        response = http::HttpResponse::okJson(root);
    }
    // --- Student Enrollment (With Automatic Group Sync) ---
    else if (path == "/api/enroll" && request.method == http::HttpMethod::POST) {
        auto json = request.getJsonBody();
        std::string student_id = json["student_id"].asString();
        std::string section_id = json["section_id"].asString();

        models::Enrollment enr;
        if (course_service_->enrollStudent(student_id, section_id, enr)) {
            auto res = utils::JsonValue::object();
            res["success"] = true;
            res["enrollment"] = enr.toJson();
            res["message"] = "Student enrolled successfully. Communication groups synchronized.";
            response = http::HttpResponse::created(res);
        } else {
            response = http::HttpResponse::badRequest("Failed to enroll: section may be full or invalid.");
        }
    }
    // --- Student Drop Course ---
    else if (path == "/api/drop" && request.method == http::HttpMethod::POST) {
        auto json = request.getJsonBody();
        std::string student_id = json["student_id"].asString();
        std::string section_id = json["section_id"].asString();

        if (course_service_->dropStudent(student_id, section_id)) {
            auto res = utils::JsonValue::object();
            res["success"] = true;
            res["message"] = "Course dropped. Removed from communication groups.";
            response = http::HttpResponse::okJson(res);
        } else {
            response = http::HttpResponse::badRequest("Failed to drop course or not enrolled.");
        }
    }
    // --- User Communication Groups ---
    else if (path == "/api/groups" && request.method == http::HttpMethod::GET) {
        std::string uid = request.getQueryParam("user_id");
        auto groups = chat_service_->getUserGroups(uid);
        auto arr = utils::JsonValue::array();
        for (const auto& g : groups) arr.push_back(g.toJson());
        auto root = utils::JsonValue::object();
        root["groups"] = arr;
        response = http::HttpResponse::okJson(root);
    }
    // --- Group Messages (GET & POST) ---
    else if (path.rfind("/api/groups/", 0) == 0 && path.find("/messages") != std::string::npos) {
        // Path format: /api/groups/{group_id}/messages
        size_t id_start = 12; // length of "/api/groups/"
        size_t id_end = path.find("/messages");
        std::string group_id = path.substr(id_start, id_end - id_start);

        if (request.method == http::HttpMethod::GET) {
            auto msgs = chat_service_->getMessages(group_id);
            auto arr = utils::JsonValue::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            auto root = utils::JsonValue::object();
            root["group_id"] = group_id;
            root["messages"] = arr;
            response = http::HttpResponse::okJson(root);
        } else if (request.method == http::HttpMethod::POST) {
            auto json = request.getJsonBody();
            std::string sender_id = json["sender_id"].asString();
            std::string sender_name = json["sender_name"].asString();
            std::string content = json["content"].asString();
            int priority = json["priority"].asInt(1);

            chat_service_->postMessage(group_id, sender_id, sender_name, content, models::MessageType::TEXT, priority);
            auto res = utils::JsonValue::object();
            res["success"] = true;
            res["message"] = "Message queued in concurrency engine";
            response = http::HttpResponse::okJson(res);
        }
    }
    // --- User Notifications ---
    else if (path == "/api/notifications" && request.method == http::HttpMethod::GET) {
        std::string uid = request.getQueryParam("user_id");
        auto notifs = notification_service_->getUserNotifications(uid);
        auto arr = utils::JsonValue::array();
        for (const auto& n : notifs) arr.push_back(n.toJson());
        auto root = utils::JsonValue::object();
        root["notifications"] = arr;
        response = http::HttpResponse::okJson(root);
    }
    // --- Instructor Announcement Fan-out ---
    else if (path == "/api/announcements" && request.method == http::HttpMethod::POST) {
        auto json = request.getJsonBody();
        std::string group_id = json["group_id"].asString();
        std::string title = json["title"].asString();
        std::string content = json["content"].asString();
        int priority = json["priority"].asInt(5);

        notification_service_->sendAnnouncement(group_id, title, content, priority);
        auto res = utils::JsonValue::object();
        res["success"] = true;
        res["message"] = "Announcement scheduled and broadcasted";
        response = http::HttpResponse::okJson(res);
    }
    else {
        response = http::HttpResponse::notFound("Endpoint not found: " + path);
    }

    epoll_server_->sendData(client_fd, response.toString());
}

void Server::handleWsFrame(int client_fd, const websocket::WsFrame& frame) {
    if (frame.opcode == websocket::WsOpCode::PING) {
        std::string pong = websocket::WebSocketParser::createPongFrame(frame.payload);
        epoll_server_->sendData(client_fd, pong);
        return;
    }

    if (frame.opcode == websocket::WsOpCode::CLOSE) {
        epoll_server_->closeClient(client_fd);
        return;
    }

    if (frame.opcode == websocket::WsOpCode::TEXT) {
        utils::JsonValue json;
        if (!utils::JsonValue::tryParse(frame.payload, json)) {
            return;
        }

        std::string action = json["action"].asString();

        if (action == "subscribe") {
            std::string group_id = json["group_id"].asString();
            websocket::WebSocketManager::getInstance().subscribe(client_fd, group_id);

            auto ack = utils::JsonValue::object();
            ack["event"] = "subscribed";
            ack["group_id"] = group_id;
            websocket::WebSocketManager::getInstance().sendText(client_fd, ack.serialize(false));
        } else if (action == "send_message") {
            std::string group_id = json["group_id"].asString();
            std::string sender_id = json["sender_id"].asString();
            std::string sender_name = json["sender_name"].asString();
            std::string content = json["content"].asString();
            int priority = json["priority"].asInt(1);

            chat_service_->postMessage(group_id, sender_id, sender_name, content, models::MessageType::TEXT, priority);
        } else if (action == "authenticate") {
            std::string token = json["token"].asString();
            std::string uid;
            models::UserRole role;
            if (auth_service_->validateToken(token, uid, role)) {
                auto session = websocket::WebSocketManager::getInstance().getSession(client_fd);
                if (session) session->setUserId(uid);

                auto auth_ack = utils::JsonValue::object();
                auth_ack["event"] = "authenticated";
                auth_ack["user_id"] = uid;
                websocket::WebSocketManager::getInstance().sendText(client_fd, auth_ack.serialize(false));
            }
        }
    }
}

void Server::handleDisconnect(int client_fd) {
    websocket::WebSocketManager::getInstance().removeSession(client_fd);
}

} // namespace server
} // namespace uniconnect