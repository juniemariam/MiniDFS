#include "storage_handler.hpp"
#include "../common/network.hpp"
#include "../common/utils.hpp"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

void handle_client(int client_fd, std::shared_ptr<StorageHandler> storage) {
    std::string line;
    if (!net::read_line(client_fd, line)) {
        net::close_socket(client_fd);
        return;
    }

    std::vector<std::string> tokens = minidfs::split_ws(line);
    if (tokens.empty()) {
        net::write_line(client_fd, "ERR empty command");
        net::close_socket(client_fd);
        return;
    }

    const std::string& command = tokens[0];
    if (command == "PING") {
        net::write_line(client_fd, "OK");
    } else if (command == "PUT" && tokens.size() == 3) {
        const std::string& chunk_id = tokens[1];
        size_t size = static_cast<size_t>(std::stoull(tokens[2]));
        std::vector<char> data(size);
        if (!data.empty() && !net::recv_all(client_fd, data.data(), data.size())) {
            net::write_line(client_fd, "ERR failed to read payload");
        } else if (storage->store_chunk(chunk_id, data)) {
            net::write_line(client_fd, "OK");
        } else {
            net::write_line(client_fd, "ERR failed to store chunk");
        }
    } else if (command == "GET" && tokens.size() == 2) {
        std::vector<char> data;
        if (!storage->retrieve_chunk(tokens[1], data)) {
            net::write_line(client_fd, "ERR missing chunk");
        } else {
            net::write_line(client_fd, "OK " + std::to_string(data.size()));
            if (!data.empty()) {
                net::send_all(client_fd, data.data(), data.size());
            }
        }
    } else if (command == "DELETE" && tokens.size() == 2) {
        if (storage->delete_chunk(tokens[1])) {
            net::write_line(client_fd, "OK");
        } else {
            net::write_line(client_fd, "ERR failed to delete chunk");
        }
    } else {
        net::write_line(client_fd, "ERR unknown command");
    }

    net::close_socket(client_fd);
}

}  // namespace

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 5001;
    std::string directory = "data/nodes/node_" + std::to_string(port);
    std::shared_ptr<StorageHandler> storage(new StorageHandler(directory));

    try {
        int server_fd = net::create_server_socket(port);
        std::cout << "Storage node listening on port " << port << " using " << directory << std::endl;

        while (true) {
            int client_fd = ::accept(server_fd, NULL, NULL);
            if (client_fd < 0) {
                continue;
            }
            std::thread(handle_client, client_fd, storage).detach();
        }
    } catch (const std::exception& ex) {
        std::cerr << "node error: " << ex.what() << std::endl;
        return 1;
    }
}

