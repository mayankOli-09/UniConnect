#include "concurrency/SharedMemoryIPC.h"

#include <windows.h>

#include <chrono>
#include <cstring>

namespace uniconnect {
namespace concurrency {

namespace {
constexpr uint32_t kMagic = 0x554E4943;
struct LockGuard {
    HANDLE h;
    explicit LockGuard(void* handle) : h(static_cast<HANDLE>(handle)) { if (h) WaitForSingleObject(h, 2000); }
    ~LockGuard() { if (h) ReleaseMutex(h); }
};
} // namespace

SharedMemoryIPC::SharedMemoryIPC(const std::string& name) : name_(name) {}
SharedMemoryIPC::~SharedMemoryIPC() { cleanup(); }

bool SharedMemoryIPC::initialize(bool create_new) {
    cleanup();
    const DWORD size = static_cast<DWORD>(sizeof(SharedMetricsSegment));
    HANDLE map = create_new ? CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, name_.c_str())
                            : OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name_.c_str());
    if (!map) return false;
    void* view = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!view) { CloseHandle(map); return false; }
    HANDLE lock = CreateMutexA(nullptr, FALSE, (name_ + "_lock").c_str());
    if (!lock) { UnmapViewOfFile(view); CloseHandle(map); return false; }
    mapping_ = map; view_ = view; lock_ = lock;
    if (create_new) {
        LockGuard g(lock_);
        auto* seg = static_cast<SharedMetricsSegment*>(view_);
        if (seg->magic != kMagic) {              // fresh mapping (another instance may already have initialised it)
            std::memset(seg, 0, sizeof(*seg));
            seg->magic = kMagic;
            seg->version = 1;
        }
    }
    return true;
}

void SharedMemoryIPC::update(const SharedMetricsSegment& metrics) {
    if (!view_) return;
    SharedMetricsSegment copy = metrics;
    copy.magic = kMagic;
    copy.version = 1;
    copy.timestamp_sec = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    LockGuard g(lock_);
    std::memcpy(view_, &copy, sizeof(copy));
}

bool SharedMemoryIPC::read(SharedMetricsSegment& out) {
    if (!view_) return false;
    LockGuard g(lock_);
    std::memcpy(&out, view_, sizeof(out));
    return out.magic == kMagic;
}

void SharedMemoryIPC::cleanup() {
    if (view_) { UnmapViewOfFile(view_); view_ = nullptr; }
    if (lock_) { CloseHandle(static_cast<HANDLE>(lock_)); lock_ = nullptr; }
    if (mapping_) { CloseHandle(static_cast<HANDLE>(mapping_)); mapping_ = nullptr; }
}

utils::JsonValue SharedMemoryIPC::readAsJson() {
    SharedMetricsSegment seg{};
    auto obj = utils::JsonValue::object();
    if (!read(seg)) { obj["status"] = "inactive"; return obj; }
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
