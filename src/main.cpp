#include "server/Server.h"

#include <iostream>

int main() {

    try {

        Server server(8080);

        server.start();

    }
    catch (const std::exception& e) {

        std::cerr << "Error: "
                  << e.what()
                  << '\n';

        return 1;
    }

    return 0;
}