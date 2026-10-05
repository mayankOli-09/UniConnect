#pragma once

#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <string>
#include "concurrency/TaskQueue.h"
#include "concurrency/Scheduler.h"
#include "utils/Json.h"

namespace uniconnect {
namespace concurrency {

enum class WorkerState {
    IDLE,
    BUSY,
    STOPPED
};

inline std::string workerStateToString(WorkerState state) {
    switch (state) {
        case WorkerState::IDLE: return "IDLE";
        case WorkerState::BUSY: return "BUSY";
        case WorkerState::STOPPED: return "STOPPED";
        default: return "IDLE";
    }
}

struct WorkerInfo {
    size_t id{0};
    std::string thread_id;
    WorkerState state{WorkerState::IDLE};
    std::string current_task_id;
    std::string current_task_name;
    uint64_t tasks_processed{0};
    double busy_time_ms{0.0};

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["id"] = id;
        obj["thread_id"] = thread_id;
        obj["state"] = workerStateToString(state);
        obj["current_task_id"] = current_task_id;
        obj["current_task_name"] = current_task_name;
        obj["tasks_processed"] = tasks_processed;
        obj["busy_time_ms"] = busy_time_ms;
        return obj;
    }
};

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = 4,
                        std::shared_ptr<Scheduler> scheduler = nullptr,
                        std::shared_ptr<TaskQueue> task_queue = nullptr);
    ~ThreadPool();

    void start();
    void stop();
    bool isRunning() const;

    void submit(Task task);

    size_t threadCount() const;
    std::shared_ptr<Scheduler> getScheduler() const;
    std::shared_ptr<TaskQueue> getTaskQueue() const;

    std::vector<WorkerInfo> getWorkersStats() const;
    utils::JsonValue getMetricsJson() const;

private:
    void workerRoutine(size_t worker_index);

    size_t num_threads_;
    std::shared_ptr<Scheduler> scheduler_;
    std::shared_ptr<TaskQueue> task_queue_;

    std::vector<std::thread> workers_;
    mutable std::mutex workers_mutex_;
    std::vector<WorkerInfo> worker_infos_;

    std::atomic<bool> running_{false};
};

} // namespace concurrency
} // namespace uniconnect
