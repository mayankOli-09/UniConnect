#include "concurrency/Dispatcher.h"

namespace uniconnect {
namespace concurrency {

Dispatcher::Dispatcher(DispatcherConfig config) : config_(config) {
    if (config_.worker_threads < 1) config_.worker_threads = 1;
    scheduler_ = std::make_shared<Scheduler>(config_.policy, config_.time_quantum_ms);
    queue_ = std::make_shared<TaskQueue>(config_.queue_capacity);
    pool_ = std::make_unique<ThreadPool>(config_.worker_threads, scheduler_, queue_);
}

Dispatcher::~Dispatcher() { stop(); }

void Dispatcher::start() {
    if (started_) return;
    pool_->start();
    started_ = true;
}

void Dispatcher::stop() {
    if (!started_) return;
    pool_->stop();
    started_ = false;
}

bool Dispatcher::submit(const std::string& name, int priority, int burst_ms, std::function<void()> work) {
    if (!started_ || scheduler_->size() >= config_.queue_capacity) {
        ++dropped_;
        return false;
    }
    const std::string id = "t" + std::to_string(++next_id_);
    pool_->submit(Task(id, name, std::move(work), priority < 1 ? 1 : (priority > 10 ? 10 : priority), burst_ms < 1 ? 1 : burst_ms));
    ++submitted_;
    const size_t depth = scheduler_->size();
    size_t peak = peak_depth_.load();
    while (depth > peak && !peak_depth_.compare_exchange_weak(peak, depth)) {}
    return true;
}

utils::JsonValue Dispatcher::statsJson() const {
    auto root = utils::JsonValue::object();
    root["thread_pool"] = pool_->getMetricsJson();
    const auto sm = scheduler_->getMetrics();
    root["scheduler"] = sm.toJson();

    // In this design the scheduler's ready queue IS the task queue, so report it as such.
    const uint64_t submitted = submitted_.load();
    const size_t depth = sm.ready_queue_length;
    auto q = utils::JsonValue::object();
    q["current_depth"] = depth;
    q["max_capacity"] = config_.queue_capacity;
    q["peak_depth"] = peak_depth_.load();
    q["total_pushed"] = submitted;
    q["total_popped"] = submitted >= depth ? submitted - depth : 0;
    q["dropped_tasks"] = dropped_.load();
    root["queue"] = q;
    return root;
}

} // namespace concurrency
} // namespace uniconnect
