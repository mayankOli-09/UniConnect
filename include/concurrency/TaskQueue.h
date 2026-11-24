#pragma once

#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <string>
#include <atomic>
#include "utils/Json.h"

namespace uniconnect {
namespace concurrency {

struct Task {
    std::string id;
    std::string name;
    int priority{1}; // 1 = low, 5 = medium, 10 = high/critical
    int estimated_burst_ms{10}; // For SJF (Shortest Job First)
    int remaining_burst_ms{10};  // For Round Robin time slicing
    std::chrono::steady_clock::time_point arrival_time;
    std::function<void()> work;

    Task() : arrival_time(std::chrono::steady_clock::now()) {}

    Task(std::string id, std::string name, std::function<void()> work, int priority = 1, int burst_ms = 10)
        : id(std::move(id)),
          name(std::move(name)),
          priority(priority),
          estimated_burst_ms(burst_ms),
          remaining_burst_ms(burst_ms),
          arrival_time(std::chrono::steady_clock::now()),
          work(std::move(work)) {}
};

struct TaskQueueMetrics {
    size_t current_depth{0};
    size_t max_capacity{0};
    size_t peak_depth{0};
    uint64_t total_pushed{0};
    uint64_t total_popped{0};
    uint64_t dropped_tasks{0};

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["current_depth"] = current_depth;
        obj["max_capacity"] = max_capacity;
        obj["peak_depth"] = peak_depth;
        obj["total_pushed"] = total_pushed;
        obj["total_popped"] = total_popped;
        obj["dropped_tasks"] = dropped_tasks;
        return obj;
    }
};

class TaskQueue {
public:
    explicit TaskQueue(size_t max_capacity = 1000);
    ~TaskQueue();

    // Blocking push: waits if queue is full until space is available or queue stopped
    bool push(Task task);

    // Push with timeout
    bool push(Task task, std::chrono::milliseconds timeout);

    // Non-blocking try push: drops task if full and increments dropped_tasks counter
    bool tryPush(Task task);

    // Blocking pop: waits if queue is empty until task arrives or queue stopped
    bool pop(Task& task);

    // Pop with timeout
    bool pop(Task& task, std::chrono::milliseconds timeout);

    void stop();
    void resume();
    bool isStopped() const;

    size_t size() const;
    size_t capacity() const;
    bool isEmpty() const;
    bool isFull() const;

    TaskQueueMetrics getMetrics() const;
    void resetMetrics();

private:
    size_t max_capacity_;
    std::deque<Task> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_cv_;
    std::condition_variable not_full_cv_;
    std::atomic<bool> stopped_{false};

    // Metrics
    size_t peak_depth_{0};
    uint64_t total_pushed_{0};
    uint64_t total_popped_{0};
    uint64_t dropped_tasks_{0};
};

} // namespace concurrency
} // namespace uniconnect
