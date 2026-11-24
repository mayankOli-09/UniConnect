#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include "concurrency/DeadlockDetector.h"
#include "concurrency/Dispatcher.h"
#include "concurrency/SharedMemoryIPC.h"
#include "models/User.h"
#include "services/ServiceResult.h"

namespace uniconnect {
namespace services {

// The system monitor: live scheduler / thread-pool / queue numbers, plus the admin controls.
class OsService {
public:
    OsService(concurrency::Dispatcher& dispatcher, concurrency::DeadlockDetector& deadlock, concurrency::SharedMemoryIPC& shm,
              std::function<size_t()> active_connections, std::function<size_t()> websocket_sessions);

    ServiceResult stats();                                                                    // any signed-in user
    ServiceResult setScheduler(const models::User& caller, const utils::JsonValue& body);     // admin
    ServiceResult deadlockTest(const models::User& caller, const utils::JsonValue& body);     // admin
    ServiceResult loadTest(const models::User& caller, const utils::JsonValue& body);         // admin

    void updateTelemetry();    // writes the current numbers to shared memory

private:
    concurrency::Dispatcher& dispatcher_;
    concurrency::DeadlockDetector& deadlock_;
    concurrency::SharedMemoryIPC& shm_;
    std::function<size_t()> active_connections_;
    std::function<size_t()> websocket_sessions_;
    std::mutex load_mutex_;
    long long last_load_ms_ = 0;
};

} // namespace services
} // namespace uniconnect
