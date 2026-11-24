#include "server/Connection.h"

#include <winsock2.h>

namespace uniconnect {
namespace server {

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() { closeSocket(); }

void Connection::appendReadData(const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    read_buffer_.append(data, len);
}

std::string Connection::peekReadBuffer() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return read_buffer_;
}

void Connection::consumeReadBuffer(size_t bytes) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (bytes >= read_buffer_.size()) read_buffer_.clear();
    else read_buffer_.erase(0, bytes);
}

void Connection::appendWriteData(const std::string& data) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    write_buffer_.append(data);
}

bool Connection::hasPendingWrite() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return !write_buffer_.empty();
}

std::string Connection::getPendingWriteData() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return write_buffer_;
}

void Connection::consumeWriteBuffer(size_t bytes) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (bytes >= write_buffer_.size()) write_buffer_.clear();
    else write_buffer_.erase(0, bytes);
}

void Connection::closeSocket() {
    if (fd_ >= 0) {
        closesocket(static_cast<SOCKET>(fd_));
        fd_ = -1;
    }
}

} // namespace server
} // namespace uniconnect
