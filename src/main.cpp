// UniConnect backend entry point (Windows).
#include <windows.h>

#include <iostream>

#include "database/MySqlAcademicRepository.h"
#include "database/MySqlChatRepository.h"
#include "database/MySqlNotificationRepository.h"
#include "database/MySqlDatabase.h"
#include "database/MySqlUserRepository.h"
#include "server/Server.h"
#include "utils/Env.h"
#include "utils/Logger.h"

using namespace uniconnect;

static server::Server* g_server = nullptr;

static BOOL WINAPI onCtrlC(DWORD) {
    if (g_server) g_server->stop();   // lets the event loop finish and clean up
    return TRUE;
}

int main() {
    auto& log = utils::Logger::getInstance();

    int port = 8080;
    const std::string portText = utils::getEnvVar("PORT");
    if (!portText.empty()) {
        try { port = std::stoi(portText); } catch (...) { log.error("Main", "PORT must be a number"); return 1; }
    }

    server::ServerConfig serverConfig;
    serverConfig.port = port;
    auto numberFrom = [&](const char* name, long lo, long hi, long& out) {
        const std::string v = utils::getEnvVar(name);
        if (v.empty()) return true;
        try { const long n = std::stol(v); if (n < lo || n > hi) throw 1; out = n; return true; }
        catch (...) { log.error("Main", name, "must be a number from", lo, "to", hi); return false; }
    };
    long workers = 4, capacity = 1000, quantum = 25;
    if (!numberFrom("WORKER_THREADS", 1, 64, workers) || !numberFrom("QUEUE_CAPACITY", 10, 100000, capacity) || !numberFrom("TIME_QUANTUM_MS", 1, 1000, quantum)) return 1;
    serverConfig.worker_threads = static_cast<size_t>(workers);
    serverConfig.queue_capacity = static_cast<size_t>(capacity);
    serverConfig.time_quantum_ms = static_cast<int>(quantum);
    serverConfig.policy = concurrency::stringToSchedulingPolicy(utils::getEnvVar("SCHEDULER_POLICY", "FCFS"));

    database::DbConfig dbConfig = database::DbConfig::fromEnvironment();
    if (dbConfig.password.empty()) {
        log.error("Main", "MYSQL_PASSWORD is not set. In PowerShell:  $env:MYSQL_PASSWORD = \"<your password>\"");
        return 1;
    }
    database::MySqlDatabase db(dbConfig);
    std::string err;
    if (!db.connect(err)) {
        log.error("Main", "Cannot connect to MySQL at", dbConfig.host + ":" + std::to_string(dbConfig.port), "-", err);
        log.error("Main", "Is the MySQL server running? Are MYSQL_USER / MYSQL_PASSWORD / MYSQL_DB correct?");
        return 1;
    }
    log.info("Main", "Connected to MySQL database", dbConfig.database);

    database::MySqlUserRepository users(db);
    database::MySqlAcademicRepository academics(db);
    database::MySqlChatRepository chat(db);
    database::MySqlNotificationRepository notifications(db);
    try {
        server::Server srv(serverConfig, server::ServerDeps{&users, &academics, &chat, &notifications});
        g_server = &srv;
        SetConsoleCtrlHandler(onCtrlC, TRUE);
        log.info("Main", "UniConnect backend ready: http://localhost:" + std::to_string(port) + "/health   (Ctrl+C to stop)");
        srv.start();
        g_server = nullptr;
    } catch (const std::exception& e) {
        log.error("Main", "Fatal:", e.what());
        return 1;
    }
    log.info("Main", "Shut down cleanly");
    return 0;
}
