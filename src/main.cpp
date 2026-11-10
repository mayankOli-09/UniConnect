#include "server/Server.h"
#include "utils/Logger.h"
#include <iostream>
#include <csignal>
#include <fstream>
#include <sstream>

static uniconnect::server::Server* g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\n[System] Caught signal " << signum << ", shutting down gracefully...\n";
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = 8080;
    size_t threads = 4;
    uniconnect::concurrency::SchedulingPolicy policy = uniconnect::concurrency::SchedulingPolicy::FCFS;

    // Load config/config.json if available
    std::ifstream cfg_file("config/config.json");
    if (!cfg_file.is_open()) {
        cfg_file.open("../config/config.json");
    }

    if (cfg_file.is_open()) {
        std::stringstream buffer;
        buffer << cfg_file.rdbuf();
        uniconnect::utils::JsonValue cfg;
        if (uniconnect::utils::JsonValue::tryParse(buffer.str(), cfg)) {
            if (cfg["server"].contains("port")) {
                port = cfg["server"]["port"].asInt(8080);
            }
            if (cfg["concurrency"].contains("thread_pool_size")) {
                threads = static_cast<size_t>(cfg["concurrency"]["thread_pool_size"].asInt(4));
            }
            if (cfg["concurrency"].contains("default_scheduler_policy")) {
                policy = uniconnect::concurrency::stringToSchedulingPolicy(
                    cfg["concurrency"]["default_scheduler_policy"].asString("FCFS")
                );
            }
        }
    }

    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {}
    }

    std::cout << "========================================================================\n"
              << "  UniConnect — Integrated University Communication Platform\n"
              << "  Team ID: OSDBMS-T109 (Avengers) | Graphic Era (Deemed to be University)\n"
              << "  C++20 Concurrency Engine: epoll Event Loop + ThreadPool + CPU Scheduler\n"
              << "========================================================================\n";

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        uniconnect::server::Server server(port, threads, policy);
        g_server = &server;

        uniconnect::utils::Logger::getInstance().info(
            "Main", "Initializing with Port:", port,
            "| Worker Threads:", threads,
            "| CPU Policy:", uniconnect::concurrency::schedulingPolicyToString(policy)
        );

        server.start();

    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << '\n';
        return 1;
    }

    std::cout << "[Main] UniConnect Server exited cleanly.\n";
    return 0;
}