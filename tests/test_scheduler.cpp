#include "concurrency/Scheduler.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace uniconnect::concurrency;

void testFCFS() {
    std::cout << "[TestScheduler] Testing FCFS Scheduling..." << std::endl;
    Scheduler sched(SchedulingPolicy::FCFS);

    sched.submit(Task("task-1", "T1", nullptr, 1, 50));
    sched.submit(Task("task-2", "T2", nullptr, 10, 10));
    sched.submit(Task("task-3", "T3", nullptr, 5, 20));

    Task t1, t2, t3;
    assert(sched.getNextTask(t1) && t1.id == "task-1");
    assert(sched.getNextTask(t2) && t2.id == "task-2");
    assert(sched.getNextTask(t3) && t3.id == "task-3");
    std::cout << " -> PASSED: Strictly executed in arrival sequence." << std::endl;
}

void testSJF() {
    std::cout << "[TestScheduler] Testing Shortest Job First (SJF)..." << std::endl;
    Scheduler sched(SchedulingPolicy::SJF);

    sched.submit(Task("long-task", "Long", nullptr, 1, 100));
    sched.submit(Task("short-task", "Short", nullptr, 1, 10));
    sched.submit(Task("med-task", "Medium", nullptr, 1, 40));

    Task t1, t2, t3;
    assert(sched.getNextTask(t1) && t1.id == "short-task");
    assert(sched.getNextTask(t2) && t2.id == "med-task");
    assert(sched.getNextTask(t3) && t3.id == "long-task");
    std::cout << " -> PASSED: Shortest burst time executed first." << std::endl;
}

void testPriorityAndAging() {
    std::cout << "[TestScheduler] Testing Priority Scheduling & Starvation Aging..." << std::endl;
    Scheduler sched(SchedulingPolicy::PRIORITY);

    sched.submit(Task("low-pri", "Low", nullptr, 1, 20));
    sched.submit(Task("high-pri", "High", nullptr, 10, 20));
    sched.submit(Task("med-pri", "Med", nullptr, 5, 20));

    Task t1, t2, t3;
    assert(sched.getNextTask(t1) && t1.id == "high-pri");
    assert(sched.getNextTask(t2) && t2.id == "med-pri");
    assert(sched.getNextTask(t3) && t3.id == "low-pri");

    // Test aging: insert low priority task, wait, and apply aging sweep
    sched.submit(Task("starving-task", "Starving", nullptr, 1, 20));
    std::this_thread::sleep_for(std::chrono::milliseconds(550)); // exceeds aging threshold 500ms
    sched.applyAging();

    SchedulerMetrics sm = sched.getMetrics();
    assert(sm.aging_promotions > 0);
    std::cout << " -> PASSED: Aging promotion successfully triggered!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   UniConnect CPU Scheduler Tests       " << std::endl;
    std::cout << "========================================" << std::endl;

    testFCFS();
    testSJF();
    testPriorityAndAging();

    std::cout << "All Scheduler tests PASSED!" << std::endl;
    return 0;
}
