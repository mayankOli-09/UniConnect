#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include "utils/Json.h"

namespace uniconnect {
namespace concurrency {

struct DeadlockReport {
    bool has_deadlock{false};
    std::vector<std::string> cycle_nodes;
    std::string victim_thread;
    std::string timestamp;

    utils::JsonValue toJson() const {
        auto obj = utils::JsonValue::object();
        obj["has_deadlock"] = has_deadlock;
        obj["victim_thread"] = victim_thread;
        obj["timestamp"] = timestamp;

        auto arr = utils::JsonValue::array();
        for (const auto& node : cycle_nodes) {
            arr.push_back(node);
        }
        obj["cycle_nodes"] = arr;
        return obj;
    }
};

class DeadlockDetector {
public:
    DeadlockDetector();

    // Resource Allocation Graph (RAG) operations
    void requestResource(const std::string& thread_id, const std::string& resource_id);
    void grantResource(const std::string& thread_id, const std::string& resource_id);
    void releaseResource(const std::string& thread_id, const std::string& resource_id);

    // Run Wait-For Graph (WFG) cycle detection algorithm
    DeadlockReport detectDeadlock();

    // Get current graph representation as JSON for dashboard
    utils::JsonValue getGraphJson();

    void clear();

private:
    std::mutex mutex_;

    // Resource -> holding thread
    std::map<std::string, std::string> resource_holder_;

    // Thread -> set of resources it is waiting for
    std::map<std::string, std::set<std::string>> thread_waiting_;

    // Cycle detection helpers
    bool dfsCycle(const std::string& current,
                  std::map<std::string, int>& color,
                  std::map<std::string, std::string>& parent,
                  std::string& cycle_start,
                  std::string& cycle_end,
                  const std::map<std::string, std::set<std::string>>& wfg);
};

} // namespace concurrency
} // namespace uniconnect
