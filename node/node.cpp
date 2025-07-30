#include <iostream>
#include <thread>
#include <netinet/in.h>

void start_node(int port) {
    std::cout << "Starting storage node on port " << port << std::endl;
    // TCP socket setup here
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 5000;
    start_node(port);
    return 0;
}
