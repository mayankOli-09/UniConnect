#pragma once

#include <string>
#include <cstdint>
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
    explicit SharedMemoryIPC(const std::string& shm_name = "/uniconnect_os_telemetry");
    ~SharedMemoryIPC();

    bool initialize(bool create_new = true);
    void update(const SharedMetricsSegment& metrics);
    bool read(SharedMetricsSegment& out_metrics);
    void cleanup();

    utils::JsonValue readAsJson();

private:
    std::string shm_name_;
    int shm_fd_{-1};
    void* mapped_addr_{nullptr};
    size_t size_{sizeof(SharedMetricsSegment)};
    bool is_creator_{false};
};

} // namespace concurrency
} // namespace uniconnect
