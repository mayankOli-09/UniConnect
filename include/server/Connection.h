#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace uniconnect {
namespace server {

enum class ConnectionType {
    HTTP,
    WEBSOCKET
};

class Connection {
public:
    explicit Connection(int fd);
    ~Connection();

    int getFd() const { return fd_; }

    ConnectionType getType() const { return type_; }
    void setType(ConnectionType type) { type_ = type; }

    void appendReadData(const char* data, size_t len);
    std::string& getReadBuffer() { return read_buffer_; }
    void consumeReadBuffer(size_t bytes);

    void appendWriteData(const std::string& data);
    bool hasPendingWrite() const;
    std::string getPendingWriteData();
    void consumeWriteBuffer(size_t bytes);

    void setUserId(const std::string& uid) { user_id_ = uid; }
    const std::string& getUserId() const { return user_id_; }

    void closeSocket();

private:
    int fd_;
    ConnectionType type_{ConnectionType::HTTP};
    std::string user_id_;

    mutable std::mutex buffer_mutex_;
    std::string read_buffer_;
    std::string write_buffer_;
};

} // namespace server
} // namespace uniconnect
