#include "concurrency/TaskQueue.h"

namespace uniconnect {
namespace concurrency {

TaskQueue::TaskQueue(size_t max_capacity)
    : max_capacity_(max_capacity == 0 ? 1000 : max_capacity) {}

TaskQueue::~TaskQueue() {
    stop();
}

void TaskQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    not_empty_cv_.notify_all();
    not_full_cv_.notify_all();
}

void TaskQueue::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = false;
}

bool TaskQueue::isStopped() const {
    return stopped_;
}

bool TaskQueue::push(Task task) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_cv_.wait(lock, [this]() {
        return stopped_ || queue_.size() < max_capacity_;
    });

    if (stopped_) {
        return false;
    }

    queue_.push_back(std::move(task));
    total_pushed_++;
    if (queue_.size() > peak_depth_) {
        peak_depth_ = queue_.size();
    }

    lock.unlock();
    not_empty_cv_.notify_one();
    return true;
}

bool TaskQueue::push(Task task, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    bool space_available = not_full_cv_.wait_for(lock, timeout, [this]() {
        return stopped_ || queue_.size() < max_capacity_;
    });

    if (stopped_ || !space_available) {
        dropped_tasks_++;
        return false;
    }

    queue_.push_back(std::move(task));
    total_pushed_++;
    if (queue_.size() > peak_depth_) {
        peak_depth_ = queue_.size();
    }

    lock.unlock();
    not_empty_cv_.notify_one();
    return true;
}

bool TaskQueue::tryPush(Task task) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_ || queue_.size() >= max_capacity_) {
        dropped_tasks_++;
        return false;
    }

    queue_.push_back(std::move(task));
    total_pushed_++;
    if (queue_.size() > peak_depth_) {
        peak_depth_ = queue_.size();
    }

    lock.unlock();
    not_empty_cv_.notify_one();
    return true;
}

bool TaskQueue::pop(Task& task) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_cv_.wait(lock, [this]() {
        return stopped_ || !queue_.empty();
    });

    if (queue_.empty()) {
        return false;
    }

    task = std::move(queue_.front());
    queue_.pop_front();
    total_popped_++;

    lock.unlock();
    not_full_cv_.notify_one();
    return true;
}

bool TaskQueue::pop(Task& task, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    bool item_available = not_empty_cv_.wait_for(lock, timeout, [this]() {
        return stopped_ || !queue_.empty();
    });

    if (!item_available || queue_.empty()) {
        return false;
    }

    task = std::move(queue_.front());
    queue_.pop_front();
    total_popped_++;

    lock.unlock();
    not_full_cv_.notify_one();
    return true;
}

size_t TaskQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

size_t TaskQueue::capacity() const {
    return max_capacity_;
}

bool TaskQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

bool TaskQueue::isFull() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size() >= max_capacity_;
}

TaskQueueMetrics TaskQueue::getMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    TaskQueueMetrics m;
    m.current_depth = queue_.size();
    m.max_capacity = max_capacity_;
    m.peak_depth = peak_depth_;
    m.total_pushed = total_pushed_;
    m.total_popped = total_popped_;
    m.dropped_tasks = dropped_tasks_;
    return m;
}

void TaskQueue::resetMetrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    total_pushed_ = 0;
    total_popped_ = 0;
    dropped_tasks_ = 0;
    peak_depth_ = queue_.size();
}

} // namespace concurrency
} // namespace uniconnect
