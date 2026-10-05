#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include "concurrency/TaskQueue.h"
#include "utils/Json.h"

namespace uniconnect {
namespace concurrency {

enum class SchedulingPolicy {
    FCFS,        // First-Come, First-Served
    SJF,         // Shortest Job First
    ROUND_ROBIN, // Round Robin (with time quantum)
    PRIORITY     // Priority Scheduling with Anti-Starvation Aging
};

inline std::string schedulingPolicyToString(SchedulingPolicy policy) {
    switch (policy) {
        case SchedulingPolicy::FCFS: return "FCFS";
        case SchedulingPolicy::SJF: return "SJF";
        case SchedulingPolicy::ROUND_ROBIN: return "ROUND_ROBIN";
        case SchedulingPolicy::PRIORITY: return "PRIORITY";
        default: return "FCFS";
    }
}

inline SchedulingPolicy stringToSchedulingPolicy(const std::string& str) {
    if (str == "SJF") return SchedulingPolicy::SJF;
    if (str == "ROUND_ROBIN" || str == "RR") return SchedulingPolicy::ROUND_ROBIN;
    if (str == "PRIORITY") return SchedulingPolicy::PRIORITY;
    return SchedulingPolicy::FCFS;
}

struct SchedulerMetrics {
    std::string policy;
    size_t ready_queue_length{0};
    uint64_t total_tasks_scheduled{0};
    uint64_t total_tasks_completed{0};
    uint64_t context_switches{0};
    uint64_t aging_promotions{0};
    double avg_waiting_time_ms{0.0};
    double avg_turnaround_time_ms{0.0};
    double avg_response_time_ms{0.0};
    int time_quantum_ms{20};

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["policy"] = policy;
        obj["ready_queue_length"] = ready_queue_length;
        obj["total_tasks_scheduled"] = total_tasks_scheduled;
        obj["total_tasks_completed"] = total_tasks_completed;
        obj["context_switches"] = context_switches;
        obj["aging_promotions"] = aging_promotions;
        obj["avg_waiting_time_ms"] = avg_waiting_time_ms;
        obj["avg_turnaround_time_ms"] = avg_turnaround_time_ms;
        obj["avg_response_time_ms"] = avg_response_time_ms;
        obj["time_quantum_ms"] = time_quantum_ms;
        return obj;
    }
};

struct ScheduledItem {
    Task task;
    uint64_t sequence_id{0};
    int effective_priority{1};
    std::chrono::steady_clock::time_point arrival_tp;
    std::chrono::steady_clock::time_point start_tp;
    bool has_started{false};
};

class Scheduler {
public:
    explicit Scheduler(SchedulingPolicy policy = SchedulingPolicy::FCFS, int time_quantum_ms = 25);
    ~Scheduler();

    void setPolicy(SchedulingPolicy policy);
    SchedulingPolicy getPolicy() const;

    void setTimeQuantum(int quantum_ms);
    int getTimeQuantum() const;

    // Enqueue a task into the ready queue
    void submit(Task task);

    // Fetch the next task according to active scheduling policy (blocks if empty)
    bool getNextTask(Task& out_task);

    // Notify scheduler of task completion for OS turnaround/waiting metrics calculation
    void recordTaskCompletion(const std::string& task_id,
                              std::chrono::steady_clock::time_point arrival_time,
                              std::chrono::steady_clock::time_point start_time);

    // Trigger aging sweep (boost priority of starvation-prone tasks in PRIORITY mode)
    void applyAging();

    void stop();
    void resume();
    bool isStopped() const;
    size_t size() const;

    SchedulerMetrics getMetrics() const;
    void resetMetrics();

private:
    SchedulingPolicy policy_;
    int time_quantum_ms_{25};
    int aging_wait_threshold_ms_{500}; // wait time before priority is incremented

    std::deque<ScheduledItem> ready_queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stopped_{false};
    uint64_t sequence_counter_{0};

    // Cumulative metrics
    uint64_t total_tasks_scheduled_{0};
    uint64_t total_tasks_completed_{0};
    uint64_t context_switches_{0};
    uint64_t aging_promotions_{0};
    double total_waiting_time_ms_{0.0};
    double total_turnaround_time_ms_{0.0};
    double total_response_time_ms_{0.0};

    // Helper selection functions
    size_t selectNextIndexLocked();
};

} // namespace concurrency
} // namespace uniconnect
