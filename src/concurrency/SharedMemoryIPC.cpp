#include "concurrency/SharedMemoryIPC.h"
#include <cstring>
#include <chrono>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace uniconnect {
namespace concurrency {

SharedMemoryIPC::SharedMemoryIPC(const std::string& shm_name)
    : shm_name_(shm_name) {}

SharedMemoryIPC::~SharedMemoryIPC() {
    cleanup();
}

bool SharedMemoryIPC::initialize(bool create_new) {
    is_creator_ = create_new;
#if !defined(_WIN32)
    int flags = create_new ? (O_CREAT | O_RDWR) : O_RDWR;
    mode_t mode = 0666;

    shm_fd_ = shm_open(shm_name_.c_str(), flags, mode);
    if (shm_fd_ == -1) {
        return false;
    }

    if (create_new) {
        if (ftruncate(shm_fd_, size_) == -1) {
            close(shm_fd_);
            shm_fd_ = -1;
            return false;
        }
    }

    mapped_addr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
    if (mapped_addr_ == MAP_FAILED) {
        mapped_addr_ = nullptr;
        close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }

    if (create_new) {
        std::memset(mapped_addr_, 0, size_);
        auto* seg = static_cast<SharedMetricsSegment*>(mapped_addr_);
        seg->magic = 0x554E4943;
        seg->version = 1;
    }
    return true;
#else
    // Simulated in-memory buffer on non-POSIX platforms
    if (!mapped_addr_) {
        mapped_addr_ = new uint8_t[size_];
        std::memset(mapped_addr_, 0, size_);
        auto* seg = static_cast<SharedMetricsSegment*>(mapped_addr_);
        seg->magic = 0x554E4943;
        seg->version = 1;
    }
    return true;
#endif
}

void SharedMemoryIPC::update(const SharedMetricsSegment& metrics) {
    if (!mapped_addr_) return;
    auto* seg = static_cast<SharedMetricsSegment*>(mapped_addr_);
    *seg = metrics;
    seg->magic = 0x554E4943;
    seg->version = 1;
    seg->timestamp_sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool SharedMemoryIPC::read(SharedMetricsSegment& out_metrics) {
    if (!mapped_addr_) return false;
    auto* seg = static_cast<SharedMetricsSegment*>(mapped_addr_);
    if (seg->magic != 0x554E4943) return false;
    out_metrics = *seg;
    return true;
}

void SharedMemoryIPC::cleanup() {
#if !defined(_WIN32)
    if (mapped_addr_ && mapped_addr_ != MAP_FAILED) {
        munmap(mapped_addr_, size_);
        mapped_addr_ = nullptr;
    }
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;
    }
    if (is_creator_) {
        shm_unlink(shm_name_.c_str());
    }
#else
    if (mapped_addr_) {
        delete[] static_cast<uint8_t*>(mapped_addr_);
        mapped_addr_ = nullptr;
    }
#endif
}

utils::JsonValue SharedMemoryIPC::readAsJson() {
    SharedMetricsSegment seg{};
    auto obj = utils::JsonValue::object();
    if (!read(seg)) {
        obj["status"] = "inactive";
        return obj;
    }

    obj["status"] = "active";
    obj["magic"] = static_cast<uint64_t>(seg.magic);
    obj["version"] = static_cast<uint64_t>(seg.version);
    obj["timestamp_sec"] = static_cast<uint64_t>(seg.timestamp_sec);
    obj["active_connections"] = static_cast<uint64_t>(seg.active_connections);
    obj["worker_threads_total"] = static_cast<uint64_t>(seg.worker_threads_total);
    obj["worker_threads_busy"] = static_cast<uint64_t>(seg.worker_threads_busy);
    obj["queue_depth"] = static_cast<uint64_t>(seg.queue_depth);
    obj["queue_capacity"] = static_cast<uint64_t>(seg.queue_capacity);
    obj["scheduler_policy"] = static_cast<uint64_t>(seg.scheduler_policy);
    obj["context_switches"] = static_cast<uint64_t>(seg.context_switches);
    obj["total_messages_processed"] = static_cast<uint64_t>(seg.total_messages_processed);
    obj["deadlock_detected"] = (seg.deadlock_detected == 1);
    obj["avg_waiting_time_ms"] = seg.avg_waiting_time_ms;
    obj["avg_turnaround_time_ms"] = seg.avg_turnaround_time_ms;
    return obj;
}

} // namespace concurrency
} // namespace uniconnect
