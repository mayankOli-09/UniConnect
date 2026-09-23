#pragma once

class Server {
public:
    explicit Server(int port);

    void start();

private:
    int port;
    int server_fd;

    void createSocket();
    void bindSocket();
    void listenForConnections();
    void acceptConnections();
    void handleClient(int client_fd);
};