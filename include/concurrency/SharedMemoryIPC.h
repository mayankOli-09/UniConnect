#pragma once
// Telemetry shared with other processes through a named Windows file mapping, guarded by a named mutex.
// Any program on the machine can open the same name and read the latest numbers.
#include <cstdint>
#include <string>

#include "utils/Json.h"

namespace uniconnect {
namespace concurrency {

#pragma pack(push, 1)
struct SharedMetricsSegment {
    uint32_t magic;                    // 0x554E4943 ('UNIC')
    uint32_t version;                  // 1
    uint64_t timestamp_sec;
    uint32_t active_connections;
    uint32_t worker_threads_total;
    uint32_t worker_threads_busy;
    uint32_t queue_depth;
    uint32_t queue_capacity;
    uint32_t scheduler_policy;         // 0: FCFS, 1: SJF, 2: RR, 3: PRIORITY
    uint64_t context_switches;
    uint64_t total_messages_processed;
    uint32_t deadlock_detected;        // 0: none, 1: deadlock active
    double avg_waiting_time_ms;
    double avg_turnaround_time_ms;
};
#pragma pack(pop)

class SharedMemoryIPC {
public:
    explicit SharedMemoryIPC(const std::string& name = "Local\\uniconnect_os_telemetry");
    ~SharedMemoryIPC();
    SharedMemoryIPC(const SharedMemoryIPC&) = delete;
    SharedMemoryIPC& operator=(const SharedMemoryIPC&) = delete;

    bool initialize(bool create_new = true);   // false: attach to an existing mapping
    void update(const SharedMetricsSegment& metrics);
    bool read(SharedMetricsSegment& out_metrics);
    void cleanup();
    utils::JsonValue readAsJson();

private:
    std::string name_;
    void* mapping_ = nullptr;
    void* view_ = nullptr;
    void* lock_ = nullptr;
};

} // namespace concurrency
} // namespace uniconnect
