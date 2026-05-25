#include "metadata_store.hpp"
#include "../common/network.hpp"
#include "../common/utils.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

class ConsistentHashRing {
public:
    explicit ConsistentHashRing(const std::vector<minidfs::NodeAddress>& nodes) {
        for (size_t i = 0; i < nodes.size(); ++i) {
            const std::string ep = minidfs::endpoint(nodes[i]);
            for (int replica = 0; replica < 64; ++replica) {
                ring_[minidfs::stable_hash(ep + "#" + std::to_string(replica))] = nodes[i];
            }
        }
    }

    std::vector<minidfs::NodeAddress> locate(const std::string& key, int replication_factor) const {
        std::vector<minidfs::NodeAddress> selected;
        std::set<std::string> seen;
        if (ring_.empty()) {
            return selected;
        }

        std::map<uint64_t, minidfs::NodeAddress>::const_iterator it = ring_.lower_bound(minidfs::stable_hash(key));
        if (it == ring_.end()) {
            it = ring_.begin();
        }

        while (selected.size() < static_cast<size_t>(replication_factor) &&
               seen.size() < count_unique_nodes()) {
            const std::string ep = minidfs::endpoint(it->second);
            if (seen.insert(ep).second) {
                selected.push_back(it->second);
            }
            ++it;
            if (it == ring_.end()) {
                it = ring_.begin();
            }
        }
        return selected;
    }

private:
    std::map<uint64_t, minidfs::NodeAddress> ring_;

    size_t count_unique_nodes() const {
        std::set<std::string> nodes;
        for (std::map<uint64_t, minidfs::NodeAddress>::const_iterator it = ring_.begin(); it != ring_.end(); ++it) {
            nodes.insert(minidfs::endpoint(it->second));
        }
        return nodes.size();
    }
};

std::vector<minidfs::NodeAddress> default_nodes() {
    std::vector<minidfs::NodeAddress> nodes;
    nodes.push_back({"127.0.0.1", 5001});
    nodes.push_back({"127.0.0.1", 5002});
    nodes.push_back({"127.0.0.1", 5003});
    return nodes;
}

std::string chunk_line(const ChunkMetadata& chunk) {
    std::ostringstream out;
    out << "CHUNK " << chunk.index << " " << chunk.chunk_id << " " << chunk.replicas.size();
    for (size_t i = 0; i < chunk.replicas.size(); ++i) {
        out << " " << minidfs::endpoint(chunk.replicas[i]);
    }
    return out.str();
}

void send_file_plan(int client_fd, const FileMetadata& file) {
    net::write_line(client_fd, "OK " + std::to_string(file.size) + " " + std::to_string(file.chunks.size()));
    for (size_t i = 0; i < file.chunks.size(); ++i) {
        net::write_line(client_fd, chunk_line(file.chunks[i]));
    }
}

void handle_client(int client_fd,
                   MetadataStore* store,
                   const ConsistentHashRing* ring,
                   int replication_factor) {
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
    if (command == "UPLOAD" && tokens.size() == 4) {
        const std::string filename = minidfs::base_name(tokens[1]);
        const size_t size = static_cast<size_t>(std::stoull(tokens[2]));
        const int chunk_count = std::stoi(tokens[3]);

        FileMetadata file;
        file.filename = filename;
        file.size = size;
        for (int i = 0; i < chunk_count; ++i) {
            ChunkMetadata chunk;
            chunk.index = i;
            chunk.chunk_id = minidfs::generate_chunk_id(filename, i);
            chunk.replicas = ring->locate(chunk.chunk_id, replication_factor);
            file.chunks.push_back(chunk);
        }
        store->put_file(file);
        send_file_plan(client_fd, file);
    } else if (command == "DOWNLOAD" && tokens.size() == 2) {
        FileMetadata file;
        if (!store->get_file(tokens[1], file)) {
            net::write_line(client_fd, "ERR file not found");
        } else {
            send_file_plan(client_fd, file);
        }
    } else if (command == "DELETE" && tokens.size() == 2) {
        FileMetadata file;
        if (!store->delete_file(tokens[1], file)) {
            net::write_line(client_fd, "ERR file not found");
        } else {
            send_file_plan(client_fd, file);
        }
    } else if (command == "LIST") {
        std::vector<FileMetadata> files = store->list_files();
        net::write_line(client_fd, "OK " + std::to_string(files.size()));
        for (size_t i = 0; i < files.size(); ++i) {
            net::write_line(client_fd, "FILE " + files[i].filename + " " +
                                      std::to_string(files[i].size) + " " +
                                      std::to_string(files[i].chunks.size()));
        }
    } else {
        net::write_line(client_fd, "ERR unknown command");
    }

    net::close_socket(client_fd);
}

}  // namespace

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::stoi(argv[1]) : minidfs::kDefaultMasterPort;
    int replication_factor = (argc > 2) ? std::stoi(argv[2]) : minidfs::kDefaultReplicationFactor;
    std::vector<minidfs::NodeAddress> nodes = default_nodes();
    replication_factor = std::max(1, std::min(replication_factor, static_cast<int>(nodes.size())));

    MetadataStore store;
    ConsistentHashRing ring(nodes);

    try {
        int server_fd = net::create_server_socket(port);
        std::cout << "Metadata master listening on port " << port
                  << " with replication factor " << replication_factor << std::endl;
        while (true) {
            int client_fd = ::accept(server_fd, NULL, NULL);
            if (client_fd < 0) {
                continue;
            }
            std::thread(handle_client, client_fd, &store, &ring, replication_factor).detach();
        }
    } catch (const std::exception& ex) {
        std::cerr << "master error: " << ex.what() << std::endl;
        return 1;
    }
}

