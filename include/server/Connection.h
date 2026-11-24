#pragma once
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>

namespace uniconnect {
namespace server {

enum class ConnectionType { HTTP, WEBSOCKET };

// One client socket plus its read/write buffers. Buffers are thread-safe (worker threads may queue writes).
class Connection {
public:
    explicit Connection(int fd);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    int getFd() const { return fd_; }
    ConnectionType getType() const { return type_; }
    void setType(ConnectionType t) { type_ = t; }

    void appendReadData(const char* data, size_t len);
    std::string peekReadBuffer() const;
    void consumeReadBuffer(size_t bytes);

    void appendWriteData(const std::string& data);
    bool hasPendingWrite() const;
    std::string getPendingWriteData() const;
    void consumeWriteBuffer(size_t bytes);

    // Close the socket once everything queued has been written (used for error replies).
    void setCloseAfterWrite() { close_after_write_ = true; }
    bool closeAfterWrite() const { return close_after_write_; }

    void closeSocket();

private:
    int fd_;
    std::atomic<ConnectionType> type_{ConnectionType::HTTP};
    std::atomic<bool> close_after_write_{false};
    mutable std::mutex buffer_mutex_;
    std::string read_buffer_;
    std::string write_buffer_;
};

} // namespace server
} // namespace uniconnect
