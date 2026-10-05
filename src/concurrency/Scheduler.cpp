#include "concurrency/Scheduler.h"
#include <algorithm>

namespace uniconnect {
namespace concurrency {

Scheduler::Scheduler(SchedulingPolicy policy, int time_quantum_ms)
    : policy_(policy), time_quantum_ms_(time_quantum_ms) {}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::setPolicy(SchedulingPolicy policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    policy_ = policy;
}

SchedulingPolicy Scheduler::getPolicy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return policy_;
}

void Scheduler::setTimeQuantum(int quantum_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    time_quantum_ms_ = quantum_ms > 0 ? quantum_ms : 20;
}

int Scheduler::getTimeQuantum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return time_quantum_ms_;
}

void Scheduler::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
}

void Scheduler::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = false;
}

bool Scheduler::isStopped() const {
    return stopped_;
}

size_t Scheduler::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ready_queue_.size();
}

void Scheduler::submit(Task task) {
    std::lock_guard<std::mutex> lock(mutex_);
    ScheduledItem item;
    item.sequence_id = ++sequence_counter_;
    item.effective_priority = task.priority;
    item.arrival_tp = std::chrono::steady_clock::now();
    item.has_started = false;
    item.task = std::move(task);

    ready_queue_.push_back(std::move(item));
    total_tasks_scheduled_++;

    cv_.notify_one();
}

void Scheduler::applyAging() {
    auto now = std::chrono::steady_clock::now();
    for (auto& item : ready_queue_) {
        auto wait_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - item.arrival_tp).count();
        if (wait_duration >= aging_wait_threshold_ms_) {
            // Anti-starvation: boost effective priority
            int boost = static_cast<int>(wait_duration / aging_wait_threshold_ms_);
            int old_pri = item.effective_priority;
            item.effective_priority = std::min(100, item.task.priority + boost);
            if (item.effective_priority > old_pri) {
                aging_promotions_++;
            }
        }
    }
}

size_t Scheduler::selectNextIndexLocked() {
    if (ready_queue_.empty()) return 0;

    switch (policy_) {
        case SchedulingPolicy::FCFS: {
            return 0; // First in arrival queue
        }

        case SchedulingPolicy::SJF: {
            // Shortest Job First: pick minimum burst time
            size_t best_idx = 0;
            int min_burst = ready_queue_[0].task.estimated_burst_ms;

            for (size_t i = 1; i < ready_queue_.size(); ++i) {
                int b = ready_queue_[i].task.estimated_burst_ms;
                if (b < min_burst || (b == min_burst && ready_queue_[i].sequence_id < ready_queue_[best_idx].sequence_id)) {
                    min_burst = b;
                    best_idx = i;
                }
            }
            return best_idx;
        }

        case SchedulingPolicy::PRIORITY: {
            // Check aging to prevent starvation
            applyAging();

            // Highest effective priority first
            size_t best_idx = 0;
            int max_priority = ready_queue_[0].effective_priority;

            for (size_t i = 1; i < ready_queue_.size(); ++i) {
                int p = ready_queue_[i].effective_priority;
                if (p > max_priority || (p == max_priority && ready_queue_[i].sequence_id < ready_queue_[best_idx].sequence_id)) {
                    max_priority = p;
                    best_idx = i;
                }
            }
            return best_idx;
        }

        case SchedulingPolicy::ROUND_ROBIN: {
            // Standard Round Robin: pick front of ready queue
            return 0;
        }
    }

    return 0;
}

bool Scheduler::getNextTask(Task& out_task) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() {
        return stopped_ || !ready_queue_.empty();
    });

    if (ready_queue_.empty()) {
        return false;
    }

    size_t chosen_idx = selectNextIndexLocked();
    ScheduledItem chosen_item = std::move(ready_queue_[chosen_idx]);
    ready_queue_.erase(ready_queue_.begin() + chosen_idx);

    auto now = std::chrono::steady_clock::now();
    if (!chosen_item.has_started) {
        chosen_item.has_started = true;
        chosen_item.start_tp = now;
        double response_ms = std::chrono::duration<double, std::milli>(now - chosen_item.arrival_tp).count();
        total_response_time_ms_ += response_ms;
    }

    context_switches_++;

    out_task = std::move(chosen_item.task);
    return true;
}

void Scheduler::recordTaskCompletion(const std::string& /*task_id*/,
                                     std::chrono::steady_clock::time_point arrival_time,
                                     std::chrono::steady_clock::time_point start_time) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);

    double turnaround = std::chrono::duration<double, std::milli>(now - arrival_time).count();
    double waiting = std::chrono::duration<double, std::milli>(start_time - arrival_time).count();

    total_turnaround_time_ms_ += turnaround;
    total_waiting_time_ms_ += waiting;
    total_tasks_completed_++;
}

SchedulerMetrics Scheduler::getMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SchedulerMetrics m;
    m.policy = schedulingPolicyToString(policy_);
    m.ready_queue_length = ready_queue_.size();
    m.total_tasks_scheduled = total_tasks_scheduled_;
    m.total_tasks_completed = total_tasks_completed_;
    m.context_switches = context_switches_;
    m.aging_promotions = aging_promotions_;
    m.time_quantum_ms = time_quantum_ms_;

    if (total_tasks_completed_ > 0) {
        m.avg_turnaround_time_ms = total_turnaround_time_ms_ / total_tasks_completed_;
        m.avg_waiting_time_ms = total_waiting_time_ms_ / total_tasks_completed_;
        m.avg_response_time_ms = total_response_time_ms_ / total_tasks_completed_;
    }

    return m;
}

void Scheduler::resetMetrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    total_tasks_scheduled_ = ready_queue_.size();
    total_tasks_completed_ = 0;
    context_switches_ = 0;
    aging_promotions_ = 0;
    total_waiting_time_ms_ = 0.0;
    total_turnaround_time_ms_ = 0.0;
    total_response_time_ms_ = 0.0;
}

} // namespace concurrency
} // namespace uniconnect
