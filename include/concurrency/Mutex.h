#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace uniconnect {
namespace concurrency {

/**
 * OS Concept: SpinLock (Busy-waiting Lock)
 * Uses atomic_flag test-and-set instruction.
 * Suitable for short critical sections where context switch overhead exceeds wait time.
 */
class SpinLock {
public:
    SpinLock() : flag_(ATOMIC_FLAG_INIT) {}

    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Hint processor to yield pipeline resources or CPU slice
            std::this_thread::yield();
        }
    }

    bool try_lock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_;
};

/**
 * OS Concept: TicketLock (Fair FIFO Mutex)
 * Solves thread starvation by issuing monotonically increasing ticket numbers,
 * analogous to deli counter tickets.
 */
class TicketLock {
public:
    TicketLock() : ticket_(0), now_serving_(0) {}

    void lock() {
        const uint32_t my_ticket = ticket_.fetch_add(1, std::memory_order_relaxed);
        while (now_serving_.load(std::memory_order_acquire) != my_ticket) {
            std::this_thread::yield();
        }
    }

    void unlock() {
        now_serving_.fetch_add(1, std::memory_order_release);
    }

private:
    std::atomic<uint32_t> ticket_;
    std::atomic<uint32_t> now_serving_;
};

/**
 * OS Concept: Readers-Writers Lock (Writer-preference to prevent writer starvation)
 */
class ReadWriteLock {
public:
    ReadWriteLock() : readers_count_(0), writer_waiting_(0), writing_(false) {}

    void read_lock() {
        std::unique_lock<std::mutex> lock(mutex_);
        read_cv_.wait(lock, [this]() {
            return !writing_ && writer_waiting_ == 0;
        });
        readers_count_++;
    }

    void read_unlock() {
        std::unique_lock<std::mutex> lock(mutex_);
        readers_count_--;
        if (readers_count_ == 0 && writer_waiting_ > 0) {
            write_cv_.notify_one();
        }
    }

    void write_lock() {
        std::unique_lock<std::mutex> lock(mutex_);
        writer_waiting_++;
        write_cv_.wait(lock, [this]() {
            return !writing_ && readers_count_ == 0;
        });
        writer_waiting_--;
        writing_ = true;
    }

    void write_unlock() {
        std::unique_lock<std::mutex> lock(mutex_);
        writing_ = false;
        if (writer_waiting_ > 0) {
            write_cv_.notify_one();
        } else {
            read_cv_.notify_all();
        }
    }

private:
    std::mutex mutex_;
    std::condition_variable read_cv_;
    std::condition_variable write_cv_;
    int readers_count_{0};
    int writer_waiting_{0};
    bool writing_{false};
};

// RAII lock guards
template<typename LockType>
class ScopedLock {
public:
    explicit ScopedLock(LockType& lock) : lock_(lock) {
        lock_.lock();
    }
    ~ScopedLock() {
        lock_.unlock();
    }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
private:
    LockType& lock_;
};

} // namespace concurrency
} // namespace uniconnect
