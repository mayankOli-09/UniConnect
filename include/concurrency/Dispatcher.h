#pragma once
// The application's use of the OS-concurrency engine: work (chat messages, announcements, ...) is turned into
// prioritized tasks, ordered by the Scheduler (FCFS / SJF / Round Robin / Priority) and run by a fixed thread pool.
#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "concurrency/Scheduler.h"
#include "concurrency/TaskQueue.h"
#include "concurrency/ThreadPool.h"
#include "utils/Json.h"

namespace uniconnect {
namespace concurrency {

struct DispatcherConfig {
    size_t worker_threads = 4;
    size_t queue_capacity = 1000;
    SchedulingPolicy policy = SchedulingPolicy::FCFS;
    int time_quantum_ms = 25;
};

class Dispatcher {
public:
    explicit Dispatcher(DispatcherConfig config = {});
    ~Dispatcher();

    void start();
    void stop();

    // Queues a task. Returns false (and counts it as dropped) when the queue is full.
    // priority 1..10 (higher runs first under the PRIORITY policy); burst_ms is the expected run time (used by SJF).
    bool submit(const std::string& name, int priority, int burst_ms, std::function<void()> work);

    Scheduler& scheduler() { return *scheduler_; }
    ThreadPool& pool() { return *pool_; }
    size_t capacity() const { return config_.queue_capacity; }

    // {"thread_pool":..., "scheduler":..., "queue":...} for the system monitor
    utils::JsonValue statsJson() const;

private:
    DispatcherConfig config_;
    std::shared_ptr<Scheduler> scheduler_;
    std::shared_ptr<TaskQueue> queue_;
    std::unique_ptr<ThreadPool> pool_;
    std::atomic<uint64_t> next_id_{0};
    std::atomic<uint64_t> submitted_{0};
    std::atomic<uint64_t> dropped_{0};
    std::atomic<size_t> peak_depth_{0};
    bool started_ = false;
};

} // namespace concurrency
} // namespace uniconnect
