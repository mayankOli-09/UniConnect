// Stage 9 check: the scheduling policies, the system-monitor endpoints and the shared-memory telemetry.
#include <winsock2.h>

#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "TestHttpClient.h"
#include "concurrency/Dispatcher.h"
#include "concurrency/SharedMemoryIPC.h"
#include "database/InMemoryUserRepository.h"
#include "server/Server.h"

namespace conc = uniconnect::concurrency;   // MSVC has its own 'Concurrency' namespace, so never write plain 'concurrency::' here
using namespace uniconnect;
using conc::SchedulingPolicy;
using utils::JsonValue;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static std::string q(const std::string& s) { return "\"" + s + "\""; }
static void pause(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// One worker is kept busy while three tasks queue up; the order they then run in shows the policy at work.
static std::vector<std::string> runOrder(SchedulingPolicy policy) {
    conc::DispatcherConfig c;
    c.worker_threads = 1;
    c.policy = policy;
    conc::Dispatcher d(c);
    d.start();
    std::mutex m;
    std::vector<std::string> done;
    std::promise<void> started, release;
    auto released = release.get_future().share();
    d.submit("BLOCKER", 1, 1, [&] { started.set_value(); released.wait(); });
    started.get_future().wait();
    auto rec = [&](const char* name) { return [&m, &done, name] { std::lock_guard<std::mutex> l(m); done.push_back(name); }; };
    d.submit("A", 9, 50, rec("A"));   // high priority, long
    d.submit("B", 1, 5, rec("B"));    // low priority, shortest
    d.submit("C", 5, 20, rec("C"));   // medium both
    release.set_value();
    for (int i = 0; i < 400; ++i) { { std::lock_guard<std::mutex> l(m); if (done.size() == 3) break; } pause(10); }
    d.stop();
    std::lock_guard<std::mutex> l(m);
    return done;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::cout << "1. each policy runs the same three tasks in its own order\n";
    CHECK(runOrder(SchedulingPolicy::FCFS) == std::vector<std::string>({"A", "B", "C"}));        // arrival order
    CHECK(runOrder(SchedulingPolicy::SJF) == std::vector<std::string>({"B", "C", "A"}));         // shortest burst first
    CHECK(runOrder(SchedulingPolicy::PRIORITY) == std::vector<std::string>({"A", "C", "B"}));    // higher number first
    CHECK(runOrder(SchedulingPolicy::ROUND_ROBIN).size() == 3);                                  // all complete

    std::cout << "2. a full queue turns work away instead of piling up\n";
    {
        conc::DispatcherConfig c;
        c.worker_threads = 1;
        c.queue_capacity = 5;
        conc::Dispatcher d(c);
        d.start();
        std::promise<void> started, release;
        auto released = release.get_future().share();
        d.submit("BLOCKER", 1, 1, [&] { started.set_value(); released.wait(); });
        started.get_future().wait();
        int accepted = 0;
        for (int i = 0; i < 5; ++i) if (d.submit("T", 1, 1, [] {})) ++accepted;
        const bool sixth = d.submit("T", 1, 1, [] {});
        const auto s = d.statsJson();
        CHECK(accepted == 5 && !sixth);
        CHECK(s["queue"]["dropped_tasks"].asInt() == 1 && s["queue"]["current_depth"].asInt() == 5 && s["queue"]["max_capacity"].asInt() == 5);
        release.set_value();
        for (int i = 0; i < 300 && d.scheduler().getMetrics().total_tasks_completed < 6; ++i) pause(10);
        CHECK(d.scheduler().getMetrics().total_tasks_completed >= 6);
        d.stop();
    }

    // ---- the server
    const int PORT = 18086;
    database::InMemoryUserRepository users;
    services::AuthConfig cfg;
    cfg.jwt_secret = "stage9-test-secret-0123456789";
    cfg.allow_privileged_signup = true;
    server::Server srv(server::ServerConfig{PORT}, server::ServerDeps{&users}, cfg);
    std::string startError;
    std::thread th([&] { try { srv.start(); } catch (const std::exception& e) { startError = e.what(); } });
    for (int i = 0; i < 300 && !srv.isRunning() && startError.empty(); ++i) pause(10);
    if (!srv.isRunning()) { std::cout << "Could not start server: " << startError << "\n"; srv.stop(); th.join(); return 1; }

    TestHttpClient c(PORT);
    TestResponse r;
    auto call = [&](const std::string& m, const std::string& path, const std::string& body, const std::string& token) {
        return c.request(m, path, body, r, token.empty() ? "" : "Authorization: Bearer " + token + "\r\n");
    };
    auto signUp = [&](const std::string& name, const std::string& email, const std::string& role) {
        c.post("/api/auth/register", "{" + q("name") + ":" + q(name) + "," + q("email") + ":" + q(email) + "," + q("password") + ":" + q("Password@123") + "," +
                                         q("role") + ":" + q(role) + "," + q("department_id") + ":" + q("1") + "}", r);
        return r.json()["token"].asString();
    };
    const std::string tStudent = signUp("Asha Rao", "asha@example.com", "STUDENT");
    const std::string tAdmin = signUp("Admin Max", "max@example.com", "ADMIN");

    std::cout << "3. sign-in required; stats have the shape the monitor expects\n";
    CHECK(call("GET", "/api/os/stats", "", "") && r.status == 401);
    CHECK(call("POST", "/api/os/scheduler", "{\"policy\":\"SJF\"}", "") && r.status == 401);
    CHECK(call("GET", "/api/os/stats", "", tStudent) && r.status == 200);
    auto j = r.json();
    CHECK(j["thread_pool"]["num_workers"].asInt() == 4 && j["thread_pool"]["workers"].size() == 4);
    CHECK(!j["thread_pool"]["workers"].at(0)["state"].asString().empty());
    CHECK(j["scheduler"]["policy"].asString() == "FCFS" && j["scheduler"].contains("context_switches") && j["scheduler"].contains("total_tasks_completed"));
    CHECK(j["queue"]["max_capacity"].asInt() == 1000 && j["queue"].contains("current_depth") && j["queue"].contains("peak_depth") && j["queue"].contains("dropped_tasks"));
    CHECK(j["deadlock"]["has_deadlock"].isNull() == false && !j["deadlock"]["has_deadlock"].asBool());
    CHECK(j["deadlock_graph"].isObject());
    CHECK(j["shared_memory"]["status"].asString() == "active" && j["shared_memory"]["worker_threads_total"].asInt() == 4);
    CHECK(j["active_connections"].asInt() >= 1 && j["ws_sessions"].asInt() == 0);

    std::cout << "4. only administrators can change anything\n";
    CHECK(call("POST", "/api/os/scheduler", "{\"policy\":\"SJF\"}", tStudent) && r.status == 403);
    CHECK(call("POST", "/api/os/deadlock-test", "{\"action\":\"create_cycle\"}", tStudent) && r.status == 403);
    CHECK(call("POST", "/api/os/load-test", "{\"count\":5}", tStudent) && r.status == 403);

    std::cout << "5. switching the scheduling policy\n";
    CHECK(call("POST", "/api/os/scheduler", "{\"policy\":\"PRIORITY\",\"time_quantum_ms\":40}", tAdmin) && r.status == 200);
    CHECK(r.json()["new_policy"].asString() == "PRIORITY" && r.json()["time_quantum_ms"].asInt() == 40);
    CHECK(call("GET", "/api/os/stats", "", tStudent) && r.json()["scheduler"]["policy"].asString() == "PRIORITY" && r.json()["shared_memory"]["scheduler_policy"].asInt() == 3);
    CHECK(call("POST", "/api/os/scheduler", "{\"policy\":\"ROUND_ROBIN\"}", tAdmin) && r.status == 200 && r.json()["new_policy"].asString() == "ROUND_ROBIN");
    CHECK(call("POST", "/api/os/scheduler", "{\"policy\":\"LOTTERY\"}", tAdmin) && r.status == 400);
    CHECK(call("POST", "/api/os/scheduler", "{\"policy\":\"SJF\",\"time_quantum_ms\":0}", tAdmin) && r.status == 400);
    CHECK(call("POST", "/api/os/scheduler", "{\"policy\":\"SJF\",\"time_quantum_ms\":5000}", tAdmin) && r.status == 400);
    CHECK(call("POST", "/api/os/scheduler", "not json", tAdmin) && r.status == 400);
    CHECK(call("POST", "/api/os/scheduler", "{\"policy\":\"FCFS\"}", tAdmin) && r.status == 200);

    std::cout << "6. deadlock detection\n";
    CHECK(call("POST", "/api/os/deadlock-test", "{\"action\":\"create_cycle\"}", tAdmin) && r.status == 200 && r.json()["has_deadlock"].asBool());
    CHECK(call("GET", "/api/os/stats", "", tStudent) && r.json()["deadlock"]["has_deadlock"].asBool() && r.json()["shared_memory"]["deadlock_detected"].asBool());
    CHECK(r.json()["deadlock_graph"].size() > 0);
    CHECK(call("POST", "/api/os/deadlock-test", "{\"action\":\"clear\"}", tAdmin) && r.status == 200 && !r.json()["has_deadlock"].asBool());
    CHECK(call("GET", "/api/os/stats", "", tStudent) && !r.json()["deadlock"]["has_deadlock"].asBool() && !r.json()["shared_memory"]["deadlock_detected"].asBool());
    CHECK(call("POST", "/api/os/deadlock-test", "{\"action\":\"explode\"}", tAdmin) && r.status == 400);

    std::cout << "7. generated load runs through the scheduler\n";
    const auto before = srv.dispatcher().scheduler().getMetrics().total_tasks_completed;
    CHECK(call("POST", "/api/os/load-test", "{\"count\":60,\"burst_ms\":10}", tAdmin) && r.status == 200 && r.json()["submitted"].asInt() == 60 && r.json()["rejected"].asInt() == 0);
    CHECK(call("POST", "/api/os/load-test", "{\"count\":5}", tAdmin) && r.status == 429);               // one at a time
    bool finished = false;
    for (int i = 0; i < 300 && !finished; ++i) { pause(50); finished = srv.dispatcher().scheduler().getMetrics().total_tasks_completed >= before + 60; }
    CHECK(finished);
    CHECK(call("GET", "/api/os/stats", "", tStudent) && r.json()["scheduler"]["total_tasks_completed"].asInt() >= 60 && r.json()["queue"]["peak_depth"].asInt() > 0);
    pause(1100);
    CHECK(call("POST", "/api/os/load-test", "{\"count\":0}", tAdmin) && r.status == 400);
    CHECK(call("POST", "/api/os/load-test", "{\"count\":501}", tAdmin) && r.status == 400);
    CHECK(call("POST", "/api/os/load-test", "{\"count\":5,\"burst_ms\":500}", tAdmin) && r.status == 400);

    std::cout << "8. another program can read the telemetry from shared memory\n";
    conc::SharedMemoryIPC peer;                                         // a second, independent handle on the same named mapping
    CHECK(peer.initialize(false));
    conc::SharedMetricsSegment seg{};
    CHECK(peer.read(seg) && seg.magic == 0x554E4943 && seg.worker_threads_total == 4 && seg.queue_capacity == 1000);
    CHECK(seg.total_messages_processed >= 60 && seg.timestamp_sec > 1700000000ull);
    CHECK(peer.readAsJson()["status"].asString() == "active");

    srv.stop();
    th.join();
    WSACleanup();
    if (failures == 0) std::cout << "\nSTAGE 9 OK\n";
    else std::cout << "\nSTAGE 9 FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
