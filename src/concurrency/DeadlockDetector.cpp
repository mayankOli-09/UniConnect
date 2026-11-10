#include "concurrency/DeadlockDetector.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace uniconnect {
namespace concurrency {

DeadlockDetector::DeadlockDetector() = default;

void DeadlockDetector::requestResource(const std::string& thread_id, const std::string& resource_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    thread_waiting_[thread_id].insert(resource_id);
}

void DeadlockDetector::grantResource(const std::string& thread_id, const std::string& resource_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    thread_waiting_[thread_id].erase(resource_id);
    resource_holder_[resource_id] = thread_id;
}

void DeadlockDetector::releaseResource(const std::string& thread_id, const std::string& resource_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = resource_holder_.find(resource_id);
    if (it != resource_holder_.end() && it->second == thread_id) {
        resource_holder_.erase(it);
    }
    thread_waiting_[thread_id].erase(resource_id);
}

void DeadlockDetector::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    resource_holder_.clear();
    thread_waiting_.clear();
}

bool DeadlockDetector::dfsCycle(const std::string& current,
                               std::map<std::string, int>& color,
                               std::map<std::string, std::string>& parent,
                               std::string& cycle_start,
                               std::string& cycle_end,
                               const std::map<std::string, std::set<std::string>>& wfg) {
    color[current] = 1; // 1 = currently exploring in recursion stack

    auto it = wfg.find(current);
    if (it != wfg.end()) {
        for (const auto& neighbor : it->second) {
            int n_color = color.count(neighbor) ? color[neighbor] : 0;
            if (n_color == 1) { // Back-edge found! Cycle detected
                cycle_start = neighbor;
                cycle_end = current;
                return true;
            }
            if (n_color == 0) {
                parent[neighbor] = current;
                if (dfsCycle(neighbor, color, parent, cycle_start, cycle_end, wfg)) {
                    return true;
                }
            }
        }
    }

    color[current] = 2; // 2 = completely visited
    return false;
}

DeadlockReport DeadlockDetector::detectDeadlock() {
    std::lock_guard<std::mutex> lock(mutex_);
    DeadlockReport report;

    // Step 1: Construct Wait-For Graph (WFG) from RAG
    // Edge (T_wait -> T_hold) exists if T_wait is waiting for R and T_hold holds R
    std::map<std::string, std::set<std::string>> wfg;
    std::set<std::string> all_threads;

    for (const auto& [thread, resources] : thread_waiting_) {
        all_threads.insert(thread);
        for (const auto& r : resources) {
            auto holder_it = resource_holder_.find(r);
            if (holder_it != resource_holder_.end()) {
                const std::string& holder = holder_it->second;
                if (holder != thread) {
                    wfg[thread].insert(holder);
                    all_threads.insert(holder);
                }
            }
        }
    }

    // Step 2: Cycle detection using 3-color DFS
    std::map<std::string, int> color; // 0=white, 1=gray, 2=black
    std::map<std::string, std::string> parent;
    std::string cycle_start, cycle_end;
    bool found_cycle = false;

    for (const auto& t : all_threads) {
        if (!color.count(t) || color[t] == 0) {
            if (dfsCycle(t, color, parent, cycle_start, cycle_end, wfg)) {
                found_cycle = true;
                break;
            }
        }
    }

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << in_time_t;
    report.timestamp = ss.str();

    if (found_cycle) {
        report.has_deadlock = true;
        std::vector<std::string> cycle;
        cycle.push_back(cycle_start);
        for (std::string curr = cycle_end; curr != cycle_start && !curr.empty(); curr = parent[curr]) {
            cycle.push_back(curr);
        }
        cycle.push_back(cycle_start);
        std::reverse(cycle.begin(), cycle.end());
        report.cycle_nodes = cycle;

        // Victim selection heuristic: pick the end node of the cycle
        report.victim_thread = cycle_end;
    }

    return report;
}

utils::JsonValue DeadlockDetector::getGraphJson() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto root = utils::JsonValue::object();

    auto nodes = utils::JsonValue::array();
    auto edges = utils::JsonValue::array();

    std::set<std::string> seen_nodes;

    for (const auto& [res, holder] : resource_holder_) {
        std::string res_label = "Resource:" + res;
        std::string thr_label = "Thread:" + holder;

        if (seen_nodes.insert(res_label).second) {
            auto node = utils::JsonValue::object();
            node["id"] = res_label;
            node["type"] = "RESOURCE";
            nodes.push_back(node);
        }
        if (seen_nodes.insert(thr_label).second) {
            auto node = utils::JsonValue::object();
            node["id"] = thr_label;
            node["type"] = "THREAD";
            nodes.push_back(node);
        }

        auto edge = utils::JsonValue::object();
        edge["from"] = res_label;
        edge["to"] = thr_label;
        edge["type"] = "ASSIGNED";
        edges.push_back(edge);
    }

    for (const auto& [thread, resources] : thread_waiting_) {
        std::string thr_label = "Thread:" + thread;
        if (seen_nodes.insert(thr_label).second) {
            auto node = utils::JsonValue::object();
            node["id"] = thr_label;
            node["type"] = "THREAD";
            nodes.push_back(node);
        }

        for (const auto& res : resources) {
            std::string res_label = "Resource:" + res;
            if (seen_nodes.insert(res_label).second) {
                auto node = utils::JsonValue::object();
                node["id"] = res_label;
                node["type"] = "RESOURCE";
                nodes.push_back(node);
            }

            auto edge = utils::JsonValue::object();
            edge["from"] = thr_label;
            edge["to"] = res_label;
            edge["type"] = "WAITING";
            edges.push_back(edge);
        }
    }

    root["nodes"] = nodes;
    root["edges"] = edges;
    return root;
}

} // namespace concurrency
} // namespace uniconnect
