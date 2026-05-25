#pragma once

#include "../common/utils.hpp"

#include <map>
#include <mutex>
#include <string>
#include <vector>

struct ChunkMetadata {
    int index;
    std::string chunk_id;
    std::vector<minidfs::NodeAddress> replicas;
};

struct FileMetadata {
    std::string filename;
    size_t size;
    std::vector<ChunkMetadata> chunks;
};

class MetadataStore {
public:
    void put_file(const FileMetadata& file) {
        std::lock_guard<std::mutex> lock(mutex_);
        files_[file.filename] = file;
    }

    bool get_file(const std::string& filename, FileMetadata& file) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<std::string, FileMetadata>::const_iterator it = files_.find(filename);
        if (it == files_.end()) {
            return false;
        }
        file = it->second;
        return true;
    }

    bool delete_file(const std::string& filename, FileMetadata& file) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<std::string, FileMetadata>::iterator it = files_.find(filename);
        if (it == files_.end()) {
            return false;
        }
        file = it->second;
        files_.erase(it);
        return true;
    }

    std::vector<FileMetadata> list_files() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<FileMetadata> files;
        for (std::map<std::string, FileMetadata>::const_iterator it = files_.begin(); it != files_.end(); ++it) {
            files.push_back(it->second);
        }
        return files;
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, FileMetadata> files_;
};

