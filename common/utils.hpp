#pragma once

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace minidfs {

static const size_t kChunkSize = 64 * 1024;
static const int kDefaultMasterPort = 4000;
static const int kDefaultReplicationFactor = 2;

struct NodeAddress {
    std::string host;
    int port;
};

inline std::string endpoint(const NodeAddress& node) {
    return node.host + ":" + std::to_string(node.port);
}

inline bool parse_endpoint(const std::string& value, NodeAddress& node) {
    size_t pos = value.find(':');
    if (pos == std::string::npos) {
        return false;
    }
    node.host = value.substr(0, pos);
    node.port = std::stoi(value.substr(pos + 1));
    return true;
}

inline std::vector<std::string> split_ws(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

inline std::string sanitize_name(const std::string& value) {
    std::string out;
    for (char c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '.' || c == '_' || c == '-') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out.empty() ? "unnamed" : out;
}

inline std::string base_name(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

inline uint64_t stable_hash(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (char c : value) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

inline std::string generate_chunk_id(const std::string& filename, int index) {
    std::ostringstream out;
    out << sanitize_name(filename) << ".chunk." << index << "." << stable_hash(filename + std::to_string(index));
    return out.str();
}

}  // namespace minidfs

