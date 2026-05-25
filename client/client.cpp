#include "cli.hpp"
#include "../common/network.hpp"
#include "../common/utils.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct ChunkPlan {
    int index;
    std::string chunk_id;
    std::vector<minidfs::NodeAddress> replicas;
};

struct FilePlan {
    size_t size;
    std::vector<ChunkPlan> chunks;
};

bool read_file(const std::string& path, std::vector<char>& data) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (size < 0) {
        return false;
    }
    data.resize(static_cast<size_t>(size));
    if (!data.empty()) {
        in.read(data.data(), size);
    }
    return static_cast<bool>(in) || in.eof();
}

bool write_file(const std::string& path, const std::vector<char>& data) {
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    if (!data.empty()) {
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    return static_cast<bool>(out);
}

bool parse_plan(int socket_fd, FilePlan& plan, std::string& error) {
    std::string line;
    if (!net::read_line(socket_fd, line)) {
        error = "master closed connection";
        return false;
    }
    std::vector<std::string> header = minidfs::split_ws(line);
    if (header.empty() || header[0] != "OK") {
        error = line;
        return false;
    }
    if (header.size() != 3) {
        error = "invalid master response: " + line;
        return false;
    }

    plan.size = static_cast<size_t>(std::stoull(header[1]));
    int chunk_count = std::stoi(header[2]);
    for (int i = 0; i < chunk_count; ++i) {
        if (!net::read_line(socket_fd, line)) {
            error = "incomplete chunk plan";
            return false;
        }
        std::vector<std::string> parts = minidfs::split_ws(line);
        if (parts.size() < 5 || parts[0] != "CHUNK") {
            error = "invalid chunk plan: " + line;
            return false;
        }

        ChunkPlan chunk;
        chunk.index = std::stoi(parts[1]);
        chunk.chunk_id = parts[2];
        int replica_count = std::stoi(parts[3]);
        if (parts.size() != static_cast<size_t>(4 + replica_count)) {
            error = "replica count mismatch";
            return false;
        }
        for (int r = 0; r < replica_count; ++r) {
            minidfs::NodeAddress node;
            if (!minidfs::parse_endpoint(parts[4 + r], node)) {
                error = "bad endpoint: " + parts[4 + r];
                return false;
            }
            chunk.replicas.push_back(node);
        }
        plan.chunks.push_back(chunk);
    }
    std::sort(plan.chunks.begin(), plan.chunks.end(),
              [](const ChunkPlan& a, const ChunkPlan& b) { return a.index < b.index; });
    return true;
}

bool request_plan(const std::string& command, FilePlan& plan, std::string& error) {
    int master_fd = net::connect_to("127.0.0.1", minidfs::kDefaultMasterPort);
    if (master_fd < 0) {
        error = "could not connect to master on port " + std::to_string(minidfs::kDefaultMasterPort);
        return false;
    }
    net::write_line(master_fd, command);
    bool ok = parse_plan(master_fd, plan, error);
    net::close_socket(master_fd);
    return ok;
}

bool put_chunk(const minidfs::NodeAddress& node, const std::string& chunk_id, const std::vector<char>& data) {
    int fd = net::connect_to(node.host, node.port);
    if (fd < 0) {
        return false;
    }
    bool ok = net::write_line(fd, "PUT " + chunk_id + " " + std::to_string(data.size()));
    if (ok && !data.empty()) {
        ok = net::send_all(fd, data.data(), data.size());
    }
    std::string response;
    ok = ok && net::read_line(fd, response) && response == "OK";
    net::close_socket(fd);
    return ok;
}

bool get_chunk(const minidfs::NodeAddress& node, const std::string& chunk_id, std::vector<char>& data) {
    int fd = net::connect_to(node.host, node.port);
    if (fd < 0) {
        return false;
    }
    bool ok = net::write_line(fd, "GET " + chunk_id);
    std::string line;
    ok = ok && net::read_line(fd, line);
    std::vector<std::string> parts = minidfs::split_ws(line);
    if (!ok || parts.size() != 2 || parts[0] != "OK") {
        net::close_socket(fd);
        return false;
    }
    size_t size = static_cast<size_t>(std::stoull(parts[1]));
    data.resize(size);
    if (!data.empty()) {
        ok = net::recv_all(fd, data.data(), data.size());
    }
    net::close_socket(fd);
    return ok;
}

void delete_chunk(const minidfs::NodeAddress& node, const std::string& chunk_id) {
    int fd = net::connect_to(node.host, node.port);
    if (fd < 0) {
        return;
    }
    net::write_line(fd, "DELETE " + chunk_id);
    std::string ignored;
    net::read_line(fd, ignored);
    net::close_socket(fd);
}

int upload(const std::string& path) {
    std::vector<char> file_data;
    if (!read_file(path, file_data)) {
        std::cerr << "Could not read file: " << path << std::endl;
        return 1;
    }

    std::string filename = minidfs::base_name(path);
    int chunk_count = static_cast<int>((file_data.size() + minidfs::kChunkSize - 1) / minidfs::kChunkSize);
    if (chunk_count == 0) {
        chunk_count = 1;
    }

    FilePlan plan;
    std::string error;
    std::ostringstream command;
    command << "UPLOAD " << filename << " " << file_data.size() << " " << chunk_count;
    if (!request_plan(command.str(), plan, error)) {
        std::cerr << "Upload failed: " << error << std::endl;
        return 1;
    }

    std::mutex mutex;
    bool failed = false;
    std::vector<std::thread> workers;
    for (size_t i = 0; i < plan.chunks.size(); ++i) {
        workers.push_back(std::thread([&, i]() {
            const ChunkPlan& chunk = plan.chunks[i];
            size_t begin = static_cast<size_t>(chunk.index) * minidfs::kChunkSize;
            size_t end = std::min(begin + minidfs::kChunkSize, file_data.size());
            std::vector<char> payload;
            if (begin < file_data.size()) {
                payload.assign(file_data.begin() + begin, file_data.begin() + end);
            }

            bool stored = false;
            for (size_t r = 0; r < chunk.replicas.size(); ++r) {
                stored = put_chunk(chunk.replicas[r], chunk.chunk_id, payload) || stored;
            }
            if (!stored) {
                std::lock_guard<std::mutex> lock(mutex);
                failed = true;
            }
        }));
    }

    for (size_t i = 0; i < workers.size(); ++i) {
        workers[i].join();
    }

    if (failed) {
        std::cerr << "Upload incomplete: one or more chunks could not be replicated" << std::endl;
        return 1;
    }
    std::cout << "Uploaded " << filename << " as " << plan.chunks.size() << " chunks" << std::endl;
    return 0;
}

int download(const std::string& filename, const std::string& output_path) {
    FilePlan plan;
    std::string error;
    if (!request_plan("DOWNLOAD " + filename, plan, error)) {
        std::cerr << "Download failed: " << error << std::endl;
        return 1;
    }

    std::vector<std::vector<char>> chunks(plan.chunks.size());
    std::mutex mutex;
    bool failed = false;
    std::vector<std::thread> workers;
    for (size_t i = 0; i < plan.chunks.size(); ++i) {
        workers.push_back(std::thread([&, i]() {
            bool ok = false;
            for (size_t r = 0; r < plan.chunks[i].replicas.size() && !ok; ++r) {
                ok = get_chunk(plan.chunks[i].replicas[r], plan.chunks[i].chunk_id, chunks[i]);
            }
            if (!ok) {
                std::lock_guard<std::mutex> lock(mutex);
                failed = true;
            }
        }));
    }
    for (size_t i = 0; i < workers.size(); ++i) {
        workers[i].join();
    }
    if (failed) {
        std::cerr << "Download failed: missing chunk replica" << std::endl;
        return 1;
    }

    std::vector<char> output;
    output.reserve(plan.size);
    for (size_t i = 0; i < chunks.size(); ++i) {
        output.insert(output.end(), chunks[i].begin(), chunks[i].end());
    }
    if (output.size() > plan.size) {
        output.resize(plan.size);
    }

    std::string target = output_path.empty() ? filename : output_path;
    if (!write_file(target, output)) {
        std::cerr << "Could not write file: " << target << std::endl;
        return 1;
    }
    std::cout << "Downloaded " << filename << " to " << target << std::endl;
    return 0;
}

int list_files() {
    int fd = net::connect_to("127.0.0.1", minidfs::kDefaultMasterPort);
    if (fd < 0) {
        std::cerr << "Could not connect to master" << std::endl;
        return 1;
    }
    net::write_line(fd, "LIST");
    std::string line;
    if (!net::read_line(fd, line)) {
        std::cerr << "List failed: no response" << std::endl;
        net::close_socket(fd);
        return 1;
    }
    std::vector<std::string> header = minidfs::split_ws(line);
    if (header.size() != 2 || header[0] != "OK") {
        std::cerr << "List failed: " << line << std::endl;
        net::close_socket(fd);
        return 1;
    }
    int count = std::stoi(header[1]);
    for (int i = 0; i < count; ++i) {
        net::read_line(fd, line);
        std::vector<std::string> parts = minidfs::split_ws(line);
        if (parts.size() == 4 && parts[0] == "FILE") {
            std::cout << parts[1] << " " << parts[2] << " bytes " << parts[3] << " chunks" << std::endl;
        }
    }
    net::close_socket(fd);
    return 0;
}

int remove_file(const std::string& filename) {
    FilePlan plan;
    std::string error;
    if (!request_plan("DELETE " + filename, plan, error)) {
        std::cerr << "Delete failed: " << error << std::endl;
        return 1;
    }

    std::vector<std::thread> workers;
    for (size_t i = 0; i < plan.chunks.size(); ++i) {
        for (size_t r = 0; r < plan.chunks[i].replicas.size(); ++r) {
            workers.push_back(std::thread(delete_chunk, plan.chunks[i].replicas[r], plan.chunks[i].chunk_id));
        }
    }
    for (size_t i = 0; i < workers.size(); ++i) {
        workers[i].join();
    }
    std::cout << "Deleted " << filename << std::endl;
    return 0;
}

void usage() {
    std::cerr << "Usage:\n"
              << "  bin/client upload <local-file>\n"
              << "  bin/client download <filename> [output-file]\n"
              << "  bin/client ls\n"
              << "  bin/client delete <filename>\n";
}

}  // namespace

int run_client(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    std::string command = argv[1];
    if (command == "upload" && argc == 3) {
        return upload(argv[2]);
    }
    if (command == "download" && (argc == 3 || argc == 4)) {
        return download(argv[2], argc == 4 ? argv[3] : "");
    }
    if ((command == "ls" || command == "list") && argc == 2) {
        return list_files();
    }
    if ((command == "delete" || command == "rm") && argc == 3) {
        return remove_file(argv[2]);
    }

    usage();
    return 1;
}

int main(int argc, char** argv) {
    return run_client(argc, argv);
}

