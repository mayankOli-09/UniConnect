#include "concurrency/TaskQueue.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>

using namespace uniconnect::concurrency;

void testBasicQueue() {
    std::cout << "[TestQueue] Running testBasicQueue..." << std::endl;
    TaskQueue queue(5);

    assert(queue.isEmpty());
    assert(queue.size() == 0);

    bool p1 = queue.push(Task("1", "T1", [](){}, 1, 10));
    bool p2 = queue.push(Task("2", "T2", [](){}, 1, 10));
    assert(p1 && p2);
    assert(queue.size() == 2);

    Task t_out;
    bool pop1 = queue.pop(t_out);
    assert(pop1 && t_out.id == "1");

    bool pop2 = queue.pop(t_out);
    assert(pop2 && t_out.id == "2");
    assert(queue.isEmpty());
    std::cout << " -> PASSED" << std::endl;
}

void testQueueBoundsAndTimeout() {
    std::cout << "[TestQueue] Running testQueueBoundsAndTimeout..." << std::endl;
    TaskQueue queue(2);

    assert(queue.push(Task("1", "T1", nullptr)));
    assert(queue.push(Task("2", "T2", nullptr)));
    assert(queue.isFull());

    // Try push when full with short timeout should fail
    bool p3 = queue.push(Task("3", "T3", nullptr), std::chrono::milliseconds(50));
    assert(!p3);

    TaskQueueMetrics m = queue.getMetrics();
    assert(m.dropped_tasks == 1);
    assert(m.peak_depth == 2);
    std::cout << " -> PASSED" << std::endl;
}

void testConcurrentProducersConsumers() {
    std::cout << "[TestQueue] Running testConcurrentProducersConsumers..." << std::endl;
    TaskQueue queue(50);
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int TASKS_PER_PRODUCER = 250;
    const int TOTAL_TASKS = NUM_PRODUCERS * TASKS_PER_PRODUCER;

    std::atomic<int> tasks_consumed{0};
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back([&queue, &tasks_consumed, TOTAL_TASKS]() {
            while (tasks_consumed < TOTAL_TASKS) {
                Task t;
                if (queue.pop(t, std::chrono::milliseconds(100))) {
                    tasks_consumed++;
                }
            }
        });
    }

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back([&queue, i, TASKS_PER_PRODUCER]() {
            for (int j = 0; j < TASKS_PER_PRODUCER; ++j) {
                std::string tid = "P" + std::to_string(i) + "_T" + std::to_string(j);
                queue.push(Task(tid, "Work", nullptr));
            }
        });
    }

    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();

    assert(tasks_consumed == TOTAL_TASKS);
    assert(queue.isEmpty());
    std::cout << " -> PASSED: Successfully processed " << tasks_consumed << " tasks concurrently!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  UniConnect OS Concurrency Queue Tests  " << std::endl;
    std::cout << "========================================" << std::endl;

    testBasicQueue();
    testQueueBoundsAndTimeout();
    testConcurrentProducersConsumers();

    std::cout << "All TaskQueue tests PASSED!" << std::endl;
    return 0;
}
