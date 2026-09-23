#include "server/Server.h"


#include <iostream>
#include <string>
#include <stdexcept>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Server::Server(int port)
    : port(port), server_fd(-1) {}

void Server::createSocket() {

    server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd == -1) {
        throw std::runtime_error("Failed to create socket");
    }

    std::cout << "[Server] Socket created\n";
}

void Server::bindSocket() {

    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(
            server_fd,
            reinterpret_cast<sockaddr*>(&server_address),
            sizeof(server_address)
        ) == -1) {

        throw std::runtime_error("Failed to bind socket");
    }

    std::cout << "[Server] Bound to port "
              << port << "\n";
}

void Server::listenForConnections() {

    if (listen(server_fd, 10) == -1) {
        throw std::runtime_error("Failed to listen");
    }

    std::cout << "[Server] Listening...\n";
}

void Server::handleClient(int client_fd) {

    char buffer[4096];

    int bytes_received = recv(
        client_fd,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    std::cout << "[Debug] recv returned: "
              << bytes_received << "\n";

    if (bytes_received < 0) {
        perror("[Server] recv");
        close(client_fd);
        return;
    }

    if (bytes_received == 0) {
        std::cout << "[Server] Client closed connection\n";
        close(client_fd);
        return;
    }

    buffer[bytes_received] = '\0';

    std::cout << "\n[HTTP Request]\n";
    std::cout << buffer << "\n";

    std::string request(buffer);

    std::string body;

    if (request.find("GET /health") == 0) {
        body = R"({"status":"ok"})";
    }
    else if (request.find("GET / ") == 0) {
        body = "UniConnect Backend Running";
    }
    else {
        body = "404 Not Found";
    }

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + body;

    std::cout << "[Debug] Sending response...\n";

    ssize_t sent = send(
        client_fd,
        response.c_str(),
        response.size(),
        0
    );

    std::cout << "[Debug] send returned: "
              << sent << "\n";

    close(client_fd);
}

void Server::acceptConnections() {

    while (true) {

        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);

        int client_fd = accept(
            server_fd,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_len
        );

        if (client_fd == -1) {

            std::cerr << "[Server] Accept failed\n";
            continue;
        }

        std::cout << "[Server] Client connected\n";

        handleClient(client_fd);
    }
}

void Server::start() {

    createSocket();
    bindSocket();
    listenForConnections();
    acceptConnections();
}