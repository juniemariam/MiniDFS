#pragma once

#include "../common/utils.hpp"

#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

class StorageHandler {
public:
    explicit StorageHandler(const std::string& directory) : directory_(directory) {
        ensure_dir("data");
        ensure_dir("data/nodes");
        ensure_dir(directory_);
    }

    bool store_chunk(const std::string& chunk_id, const std::vector<char>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream out(path_for(chunk_id).c_str(), std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        return static_cast<bool>(out);
    }

    bool retrieve_chunk(const std::string& chunk_id, std::vector<char>& data) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ifstream in(path_for(chunk_id).c_str(), std::ios::binary);
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

    bool delete_chunk(const std::string& chunk_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return ::remove(path_for(chunk_id).c_str()) == 0 || errno == ENOENT;
    }

private:
    std::string directory_;
    mutable std::mutex mutex_;

    static void ensure_dir(const std::string& directory) {
        if (::mkdir(directory.c_str(), 0755) != 0 && errno != EEXIST) {
            throw std::runtime_error("failed to create " + directory);
        }
    }

    std::string path_for(const std::string& chunk_id) const {
        return directory_ + "/" + minidfs::sanitize_name(chunk_id);
    }
};
