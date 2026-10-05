#include "concurrency/ThreadPool.h"
#include <sstream>

namespace uniconnect {
namespace concurrency {

ThreadPool::ThreadPool(size_t num_threads,
                       std::shared_ptr<Scheduler> scheduler,
                       std::shared_ptr<TaskQueue> task_queue)
    : num_threads_(num_threads == 0 ? 4 : num_threads),
      scheduler_(scheduler),
      task_queue_(task_queue) {

    if (!scheduler_ && !task_queue_) {
        scheduler_ = std::make_shared<Scheduler>();
    }

    worker_infos_.resize(num_threads_);
    for (size_t i = 0; i < num_threads_; ++i) {
        worker_infos_[i].id = i;
        worker_infos_[i].state = WorkerState::IDLE;
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    if (running_.exchange(true)) {
        return;
    }

    workers_.reserve(num_threads_);
    for (size_t i = 0; i < num_threads_; ++i) {
        workers_.emplace_back(&ThreadPool::workerRoutine, this, i);
    }
}

void ThreadPool::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (scheduler_) scheduler_->stop();
    if (task_queue_) task_queue_->stop();

    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
    workers_.clear();

    std::lock_guard<std::mutex> lock(workers_mutex_);
    for (auto& info : worker_infos_) {
        info.state = WorkerState::STOPPED;
    }
}

bool ThreadPool::isRunning() const {
    return running_;
}

void ThreadPool::submit(Task task) {
    if (scheduler_) {
        scheduler_->submit(std::move(task));
    } else if (task_queue_) {
        task_queue_->push(std::move(task));
    }
}

size_t ThreadPool::threadCount() const {
    return num_threads_;
}

std::shared_ptr<Scheduler> ThreadPool::getScheduler() const {
    return scheduler_;
}

std::shared_ptr<TaskQueue> ThreadPool::getTaskQueue() const {
    return task_queue_;
}

void ThreadPool::workerRoutine(size_t worker_index) {
    {
        std::ostringstream ss;
        ss << std::this_thread::get_id();
        std::lock_guard<std::mutex> lock(workers_mutex_);
        worker_infos_[worker_index].thread_id = ss.str();
    }

    while (running_) {
        Task task;
        bool got_task = false;

        if (scheduler_) {
            got_task = scheduler_->getNextTask(task);
        } else if (task_queue_) {
            got_task = task_queue_->pop(task, std::chrono::milliseconds(200));
        }

        if (!got_task) {
            continue;
        }

        auto start_tp = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(workers_mutex_);
            worker_infos_[worker_index].state = WorkerState::BUSY;
            worker_infos_[worker_index].current_task_id = task.id;
            worker_infos_[worker_index].current_task_name = task.name;
        }

        try {
            if (task.work) {
                task.work();
            }
        } catch (...) {
            // Guard against uncaught worker exceptions
        }

        auto finish_tp = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(finish_tp - start_tp).count();

        if (scheduler_) {
            scheduler_->recordTaskCompletion(task.id, task.arrival_time, start_tp);
        }

        {
            std::lock_guard<std::mutex> lock(workers_mutex_);
            worker_infos_[worker_index].state = WorkerState::IDLE;
            worker_infos_[worker_index].current_task_id.clear();
            worker_infos_[worker_index].current_task_name.clear();
            worker_infos_[worker_index].tasks_processed++;
            worker_infos_[worker_index].busy_time_ms += elapsed_ms;
        }
    }
}

std::vector<WorkerInfo> ThreadPool::getWorkersStats() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    return worker_infos_;
}

utils::JsonValue ThreadPool::getMetricsJson() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    auto root = utils::JsonValue::object();
    root["num_workers"] = num_threads_;
    root["is_running"] = running_.load();

    auto workers_arr = utils::JsonValue::array();
    size_t busy_count = 0;
    uint64_t total_tasks = 0;

    for (const auto& w : worker_infos_) {
        if (w.state == WorkerState::BUSY) busy_count++;
        total_tasks += w.tasks_processed;
        workers_arr.push_back(w.toJson());
    }

    root["busy_workers"] = busy_count;
    root["idle_workers"] = num_threads_ - busy_count;
    root["total_tasks_processed"] = total_tasks;
    root["workers"] = workers_arr;

    if (scheduler_) {
        root["scheduler"] = scheduler_->getMetrics().toJson();
    }
    if (task_queue_) {
        root["queue"] = task_queue_->getMetrics().toJson();
    }

    return root;
}

} // namespace concurrency
} // namespace uniconnect
