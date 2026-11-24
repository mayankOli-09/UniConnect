#include "services/OsService.h"

#include <chrono>
#include <random>
#include <thread>

namespace uniconnect {
namespace services {

using utils::JsonValue;

namespace {
ServiceResult failure(int status, const std::string& msg) {
    ServiceResult r;
    r.status = status;
    r.error = msg;
    return r;
}
bool isAdmin(const models::User& u) { return u.role == models::UserRole::ADMIN; }
bool knownPolicy(const std::string& p) { return p == "FCFS" || p == "SJF" || p == "ROUND_ROBIN" || p == "RR" || p == "PRIORITY"; }
long long nowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
} // namespace

OsService::OsService(concurrency::Dispatcher& d, concurrency::DeadlockDetector& dl, concurrency::SharedMemoryIPC& shm,
                     std::function<size_t()> conns, std::function<size_t()> ws)
    : dispatcher_(d), deadlock_(dl), shm_(shm), active_connections_(std::move(conns)), websocket_sessions_(std::move(ws)) {}

void OsService::updateTelemetry() {
    concurrency::SharedMetricsSegment seg{};
    seg.active_connections = static_cast<uint32_t>(active_connections_ ? active_connections_() : 0);
    seg.worker_threads_total = static_cast<uint32_t>(dispatcher_.pool().threadCount());
    uint32_t busy = 0;
    for (const auto& w : dispatcher_.pool().getWorkersStats()) if (w.state == concurrency::WorkerState::BUSY) ++busy;
    seg.worker_threads_busy = busy;
    const auto sm = dispatcher_.scheduler().getMetrics();
    seg.queue_depth = static_cast<uint32_t>(sm.ready_queue_length);
    seg.queue_capacity = static_cast<uint32_t>(dispatcher_.capacity());
    seg.scheduler_policy = static_cast<uint32_t>(dispatcher_.scheduler().getPolicy());
    seg.context_switches = sm.context_switches;
    seg.total_messages_processed = sm.total_tasks_completed;
    seg.avg_waiting_time_ms = sm.avg_waiting_time_ms;
    seg.avg_turnaround_time_ms = sm.avg_turnaround_time_ms;
    seg.deadlock_detected = deadlock_.detectDeadlock().has_deadlock ? 1 : 0;
    shm_.update(seg);
}

ServiceResult OsService::stats() {
    updateTelemetry();
    ServiceResult r;
    r.body = dispatcher_.statsJson();   // thread_pool, scheduler, queue
    r.body["deadlock"] = deadlock_.detectDeadlock().toJson();
    r.body["deadlock_graph"] = deadlock_.getGraphJson();
    r.body["shared_memory"] = shm_.readAsJson();
    r.body["active_connections"] = active_connections_ ? active_connections_() : 0;
    r.body["ws_sessions"] = websocket_sessions_ ? websocket_sessions_() : 0;
    return r;
}

ServiceResult OsService::setScheduler(const models::User& caller, const JsonValue& body) {
    if (!isAdmin(caller)) return failure(403, "Only administrators can change the scheduling policy.");
    const std::string policy = body["policy"].asString();
    if (!knownPolicy(policy)) return failure(400, "policy must be one of FCFS, SJF, ROUND_ROBIN, PRIORITY.");
    int quantum = dispatcher_.scheduler().getTimeQuantum();
    if (body.contains("time_quantum_ms")) {
        quantum = body["time_quantum_ms"].asInt(-1);
        if (quantum < 1 || quantum > 1000) return failure(400, "time_quantum_ms must be a number from 1 to 1000.");
    }
    dispatcher_.scheduler().setPolicy(concurrency::stringToSchedulingPolicy(policy));
    dispatcher_.scheduler().setTimeQuantum(quantum);

    ServiceResult r;
    r.body = JsonValue::object();
    r.body["success"] = true;
    r.body["new_policy"] = concurrency::schedulingPolicyToString(dispatcher_.scheduler().getPolicy());
    r.body["time_quantum_ms"] = dispatcher_.scheduler().getTimeQuantum();
    return r;
}

ServiceResult OsService::deadlockTest(const models::User& caller, const JsonValue& body) {
    if (!isAdmin(caller)) return failure(403, "Only administrators can run the deadlock test.");
    const std::string action = body["action"].asString("create_cycle");
    if (action != "create_cycle" && action != "clear") return failure(400, "action must be create_cycle or clear.");
    deadlock_.clear();
    if (action == "create_cycle") {
        // A circular wait: T1 holds A and wants B, T2 holds B and wants A.
        deadlock_.grantResource("WorkerThread-1", "Resource-MutexA");
        deadlock_.requestResource("WorkerThread-1", "Resource-MutexB");
        deadlock_.grantResource("WorkerThread-2", "Resource-MutexB");
        deadlock_.requestResource("WorkerThread-2", "Resource-MutexA");
    }
    ServiceResult r;
    r.body = deadlock_.detectDeadlock().toJson();
    return r;
}

// Submits a burst of synthetic tasks with random priorities so the scheduler's behaviour becomes visible on the monitor.
ServiceResult OsService::loadTest(const models::User& caller, const JsonValue& body) {
    if (!isAdmin(caller)) return failure(403, "Only administrators can generate test load.");
    int count = 50, burst = 20;
    if (body.contains("count")) count = body["count"].asInt(-1);
    if (body.contains("burst_ms")) burst = body["burst_ms"].asInt(-1);
    if (count < 1 || count > 500) return failure(400, "count must be a number from 1 to 500.");
    if (burst < 1 || burst > 200) return failure(400, "burst_ms must be a number from 1 to 200.");
    {
        std::lock_guard<std::mutex> l(load_mutex_);
        if (nowMs() - last_load_ms_ < 1000) return failure(429, "Please wait a second between load tests.");
        last_load_ms_ = nowMs();
    }
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> prio(1, 10), jitter(1, burst);
    int accepted = 0, rejected = 0;
    for (int i = 0; i < count; ++i) {
        const int p = prio(rng), b = jitter(rng);
        if (dispatcher_.submit("LOAD_TEST_" + std::to_string(i), p, b, [b]() { std::this_thread::sleep_for(std::chrono::milliseconds(b)); })) ++accepted;
        else ++rejected;
    }
    ServiceResult r;
    r.body = JsonValue::object();
    r.body["success"] = true;
    r.body["submitted"] = accepted;
    r.body["rejected"] = rejected;
    return r;
}

} // namespace services
} // namespace uniconnect
