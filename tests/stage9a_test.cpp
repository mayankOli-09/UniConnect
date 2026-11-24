// Stage 9a check: the deadlock detector and the Windows shared-memory telemetry.
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include <windows.h>

#include "concurrency/DeadlockDetector.h"
#include "concurrency/SharedMemoryIPC.h"

using namespace uniconnect;
using namespace uniconnect::concurrency;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

int main() {
    std::cout << "1. deadlock detector: no false alarms\n";
    DeadlockDetector d;
    CHECK(!d.detectDeadlock().has_deadlock);
    d.grantResource("T1", "A");
    d.requestResource("T2", "A");                        // T2 simply waits for T1: a chain, not a cycle
    CHECK(!d.detectDeadlock().has_deadlock);
    d.requestResource("T3", "A");
    CHECK(!d.detectDeadlock().has_deadlock);
    d.releaseResource("T1", "A");
    CHECK(!d.detectDeadlock().has_deadlock);

    std::cout << "2. a circular wait is found, and its report has the fields the monitor shows\n";
    d.clear();
    d.grantResource("T1", "A");
    d.requestResource("T1", "B");
    d.grantResource("T2", "B");
    d.requestResource("T2", "A");
    const auto rep = d.detectDeadlock();
    CHECK(rep.has_deadlock && rep.cycle_nodes.size() >= 2 && !rep.victim_thread.empty() && !rep.timestamp.empty());
    const auto j = rep.toJson();
    CHECK(j["has_deadlock"].asBool() && j["cycle_nodes"].size() >= 2 && !j["victim_thread"].asString().empty());
    const auto g = d.getGraphJson();
    CHECK(g.isObject() && g.size() > 0);

    std::cout << "3. releasing a resource breaks the cycle; clear() resets everything\n";
    d.releaseResource("T2", "B");
    CHECK(!d.detectDeadlock().has_deadlock);
    d.requestResource("T1", "B");
    d.clear();
    CHECK(!d.detectDeadlock().has_deadlock);

    std::cout << "4. a three-way cycle\n";
    d.grantResource("T1", "A"); d.requestResource("T1", "B");
    d.grantResource("T2", "B"); d.requestResource("T2", "C");
    d.grantResource("T3", "C"); d.requestResource("T3", "A");
    CHECK(d.detectDeadlock().has_deadlock && d.detectDeadlock().cycle_nodes.size() >= 3);

    std::cout << "5. shared memory: nothing to read until it is created\n";
    const std::string name = "Local\\uniconnect_test_shm_" + std::to_string(GetCurrentProcessId());
    SharedMemoryIPC writer(name), reader(name);
    CHECK(writer.readAsJson()["status"].asString() == "inactive");
    CHECK(!reader.initialize(false));                                  // attaching to a mapping that does not exist fails
    SharedMetricsSegment none{};
    CHECK(!writer.read(none));

    std::cout << "6. a second handle sees what the first one writes\n";
    CHECK(writer.initialize(true));
    CHECK(reader.initialize(false));
    SharedMetricsSegment s{};
    s.active_connections = 7; s.worker_threads_total = 4; s.worker_threads_busy = 2; s.queue_depth = 9; s.queue_capacity = 1000;
    s.scheduler_policy = 3; s.context_switches = 123456789012ull; s.total_messages_processed = 42; s.deadlock_detected = 1;
    s.avg_waiting_time_ms = 12.5; s.avg_turnaround_time_ms = 30.25;
    writer.update(s);
    SharedMetricsSegment got{};
    CHECK(reader.read(got) && got.magic == 0x554E4943 && got.version == 1);
    CHECK(got.active_connections == 7 && got.worker_threads_total == 4 && got.worker_threads_busy == 2 && got.queue_depth == 9 && got.queue_capacity == 1000);
    CHECK(got.scheduler_policy == 3 && got.context_switches == 123456789012ull && got.total_messages_processed == 42 && got.deadlock_detected == 1);
    CHECK(got.avg_waiting_time_ms == 12.5 && got.avg_turnaround_time_ms == 30.25 && got.timestamp_sec > 1700000000ull);
    const auto js = reader.readAsJson();
    CHECK(js["status"].asString() == "active" && js["scheduler_policy"].asInt() == 3 && js["deadlock_detected"].asBool() && js["avg_waiting_time_ms"].asDouble() == 12.5);

    std::cout << "7. readers never see a half-written update (thousands of updates while another thread reads)\n";
    { SharedMetricsSegment w0{}; w0.context_switches = 0; w0.total_messages_processed = 0; writer.update(w0); }   // start from a consistent pair
    std::atomic<bool> stop{false};
    std::atomic<int> torn{0}, reads{0};
    std::thread t([&] {
        SharedMetricsSegment x{};
        while (!stop) {
            if (reader.read(x)) { ++reads; if (x.context_switches != x.total_messages_processed) ++torn; }   // the two fields are always written together
        }
    });
    // Keep writing until the reader has really been reading (at least 2000 updates and 500 reads, within 5 seconds).
    const auto started = std::chrono::steady_clock::now();
    uint64_t updates = 0;
    while ((updates < 2000 || reads < 500) && std::chrono::steady_clock::now() - started < std::chrono::seconds(5)) {
        ++updates;
        SharedMetricsSegment w{};
        w.context_switches = updates;
        w.total_messages_processed = updates;
        writer.update(w);
    }
    stop = true;
    t.join();
    CHECK(torn == 0 && reads >= 500);
    std::cout << "   " << updates << " updates, " << reads << " reads, " << torn << " torn\n";

    std::cout << "8. cleanup\n";
    reader.cleanup();
    writer.cleanup();
    SharedMemoryIPC late(name);
    CHECK(!late.initialize(false));                                    // the mapping disappears when the last handle closes

    if (failures == 0) std::cout << "\nSTAGE 9a OK\n";
    else std::cout << "\nSTAGE 9a FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
